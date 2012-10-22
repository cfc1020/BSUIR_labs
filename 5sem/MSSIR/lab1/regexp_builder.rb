class RegexpBuilder

  def self.b_ops
    %w[+= -= == != /= *= |= &= ^= && || + - * / = | & ^].map{ |ch| Regexp.escape ch }.join(?|)
  end

  def self.u_ops
    %w[- & ~ !].map{ |ch| Regexp.escape ch }.join(?|)
  end

  DEF = %q{
    (?<class_def> \g<mod> \s+ class \s+ \g<identifier> \s* \{ ( \g<field_def> | \g<method_def> | \s+ )* \} ){0}
    (?<field_def> (\g<mod> \s+  )+ \g<identifier> \s+ \g<def_or_init> ( \s* , \s* \g<def_or_init> )* \s*; ){0}
    (?<method_def> (\g<mod> \s+ )+ (?<ex_name> ( \g<identifier> \s* ){1,2} ) \( (?<params> (\s* \g<identifier> \s*,?)* ) \s* \) \s* (\g<block>) ){0}
    (?<block> \{ \s* ( \g<operation>  \s* )* \} ){0}
    (?<method_call> \g<identifier> \s* \( ( \s* \g<expr> (\s* , \s* \g<expr> )* )? \s* \) ){0}
    (?<expr>  \g<var> \s* \g<b_op> \s* \g<expr> | \g<u_op> \s* \g<expr> | \( \s* \g<expr> \s* \) | \g<method_call> | \g<identifier> ){0}
    (?<operation>  ( \g<if_form> | \g<for_form> | \g<block> | \g<var_def> | ( \g<expr> \s* )? ; ) ){0}
    (?<if_form> if \s* \( \s* \g<expr> \s* \) \s* (?<then_block> \g<operation> )
      ( \s* else (?<else_block> \g<operation> ) )? ){0}
    (?<for_form> for \s* \( ( \s* \g<expr> \s* ; ){2} \s* \g<expr> \) \g<operation> ){0}
    (?<mod> (public | private | protected | abstract | static | final) ){0}
    (?<def_or_init> \g<identifier> ( \s* = \s* \g<expr> )? ){0}
    (?<var_def> (\g<mod> \s+ )? \g<type> \s+ (?<defs>  \g<def_or_init> ( \s*, \s* \g<def_or_init> )* \s* ) ; ){0}
    (?<var> \w+ ){0}
    (?<type> \g<identifier> ){0}
    (?<identifier> [_a-zA-Z]\w* ){0}
  }.strip! +
  %Q{
    (?<b_op> (#{b_ops}) ){0}
    (?<u_op> (#{u_ops}) ){0}
  }.strip!

  class << self
    def make_regexp expr
      Regexp.compile(DEF + expr, Regexp::EXTENDED | Regexp::MULTILINE)
    end

    %w[class_def method_def field_def expr].each do |s|
      define_method(s){ make_regexp " \\g<#{s}>" }
    end

    def io_operations str
      re = make_regexp('(?<wr> \g<identifier> ) \s* = \s* \g<expr>')
      res = {i: [], o: []}
      str.gsub(re) do |s|
        m = re.match(str)
        res.merge!(io_operation(m[:expr])){ |_, *v| v.flatten }
        res[:o] << m[:wr]
        m[:expr].gsub(make_regexp '\g<identifier>') do |i|
          res[:i] << i
        end
      end
      res
    end

    def surrounded_by b, e
      # Note: b, e - one character strings.
      @sur_i ||= 0
      @sur_i += 1
      /(?<ent#@sur_i> #{Regexp.escape b} (?: (?> [^#{b}#{e}]+ ) | \g<ent#@sur_i> )* #{Regexp.escape e} )/x
    end
  end
end