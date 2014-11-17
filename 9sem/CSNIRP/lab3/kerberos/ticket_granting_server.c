#include <winsock2.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "crypt.h"

BOOL exited = FALSE;

SOCKET initialize_server_socket()
{
  WORD sockVer;
  WSADATA wsaData;
  int retVal;

  sockVer = MAKEWORD(2,2);

  WSAStartup(sockVer, &wsaData);

  SOCKET servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (servSock == INVALID_SOCKET) {
    fprintf(stderr, "Unable to create socket\n");
    WSACleanup();
    exit(-1);
  }

  SOCKADDR_IN sin;
  sin.sin_family = PF_INET;
  sin.sin_port = htons(TGS_PORT);
  sin.sin_addr.s_addr = INADDR_ANY;

  retVal = bind(servSock, (LPSOCKADDR)&sin, sizeof(sin));
  if (retVal == SOCKET_ERROR) {
    fprintf(stderr, "Unable to bind\n");
    WSACleanup();
    exit(-1);
  }

  retVal = listen(servSock, 10);
  if (retVal == SOCKET_ERROR) {
    fprintf(stderr, "Unable to listen\n");
    WSACleanup();
    exit(-1);
  }

  return servSock;
}

void process_client(SOCKET sock, HCRYPTPROV prov, HCRYPTKEY tgs_key, HCRYPTKEY r_key)
{
  printf("---------- Start processing client ----------\n");
  int retVal;
  char* buf = malloc(10240);
  size_t msg_len = 0;

  msg_len = retVal = recv(sock, buf, 10240, 0);
  if (retVal == SOCKET_ERROR) {
    fprintf(stderr, "Unable to recv\n");
    exit(-1);
  }
  DWORD auth_enc_len = msg_len;
  BYTE* auth_enc = malloc(auth_enc_len);
  memcpy(auth_enc, buf, auth_enc_len);
  printf("Got 1st part (authenticator). Waiting for TGS mandate to decrypt. Encrypted: ");
  print_hex_binary(auth_enc, auth_enc_len);

  msg_len = retVal = recv(sock, buf, 10240, 0);
  if (retVal == SOCKET_ERROR) {
    fprintf(stderr, "Unable to recv\n");
    exit(-1);
  }

  printf("Got 2nd part (TGS mandate). Encrypted: ");
  print_hex_binary(buf, msg_len);
  struct Mandate tgs_mandate = parse_mandate(buf, msg_len, tgs_key);
  HCRYPTKEY session_key;
  get_key_from_binary(tgs_mandate.session_key_blob, tgs_mandate.session_key_size, prov, &session_key);
  printf("Decrypting authenticator. ");
  struct Authenticator auth = parse_authenticator(auth_enc, auth_enc_len, session_key);

  printf("Asserting server name equal: %s == %s\n", tgs_mandate.server_name, "TGS");
  if (strcmp(tgs_mandate.server_name, "TGS") != 0)
  {
    printf("Server name doesn't match. Client messed up servers?\n");
    free(auth_enc);
    free(buf);
    closesocket(sock);
    return;
  }

  printf("Asserting client name equal: %s == %s\n", tgs_mandate.user_name, auth.user_name);
  if (strcmp(tgs_mandate.user_name, auth.user_name) != 0)
  {
    printf("NAMES DON'T MATCH, MALICIOUS USER DETECTED!\n");
    free(auth_enc);
    free(buf);
    closesocket(sock);
    return;
  }

  printf("Asserting time not expired: %u < %u\n", auth.now, tgs_mandate.expires);
  if (auth.now >= tgs_mandate.expires)
  {
    printf("Mandate expired, client needs a new one.\n");
    free(auth_enc);
    free(buf);
    closesocket(sock);
    return;
  }

  printf("Generate new session key\n");
  DWORD session_key_size = 260;
  BYTE session_key_blob[session_key_size];
  generate_session_key(prov, &session_key_size, session_key_blob);

  printf("Session key plain: ");
  print_hex_binary(session_key_blob, session_key_size);
  msg_len = do_crypt(session_key, ENCODE, session_key_blob, buf, session_key_size, 10240);
  printf("Sending 1st part of the message (encrypted session key): ");
  print_hex_binary(buf, msg_len);
  retVal = send(sock, buf, msg_len, 0);
  if (retVal == SOCKET_ERROR) {
    fprintf(stderr, "Unable to send\n");
    exit(-1);
  }

  struct Mandate r_mandate = {"RS", auth.user_name, time(NULL) + 24 * 60 * 60, session_key_blob, session_key_size};
  msg_len = create_mandate(buf, r_mandate, r_key);
  printf("Sending 2nd part of the message (encrypted RS mandate): ");
  print_hex_binary(buf, msg_len);
  retVal = send(sock, buf, msg_len, 0);
  if (retVal == SOCKET_ERROR) {
    fprintf(stderr, "Unable to send\n");
    exit(-1);
  }

  printf("---------- Done processing client ----------\n");
  free(auth_enc);
  free(buf);
  closesocket(sock);
}

void server_loop(SOCKET sock, HCRYPTPROV prov, HCRYPTKEY tgs_key, HCRYPTKEY r_key)
{
  SOCKET clientSock;
  while (!exited) {
    fd_set rfds;
    struct timeval tv;
    int retVal;

    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    retVal = select(1, &rfds, NULL, NULL, &tv);
    if (retVal == -1) {
      fprintf(stderr, "Select error\n");
      WSACleanup();
      exit(-1);
    } else if (retVal) {
      clientSock = accept(sock, NULL, NULL);
      if (clientSock == INVALID_SOCKET) {
        fprintf(stderr, "Unable to accept\n");
        WSACleanup();
        exit(-1);
      }

      process_client(clientSock, prov, tgs_key, r_key);
    }
  }
}

void trap_exit(int signal)
{
  exited = TRUE;
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <tgs_key_file> <r_key_file>", argv[0]);
    return -1;
  }

  HCRYPTPROV prov;
  create_prov(&prov);

  HCRYPTKEY tgs_key;
  load_binary_key_from_file(argv[1], prov, &tgs_key);

  HCRYPTKEY r_key;
  load_binary_key_from_file(argv[2], prov, &r_key);

  SOCKET sock = initialize_server_socket();

  signal(SIGINT, trap_exit);
  server_loop(sock, prov, tgs_key, r_key);

  closesocket(sock);
  finalize_prov(prov);

  return 0;
}
