#include "lexer.h"
#include <algorithm>

using namespace std;

namespace parse {

    bool operator==(const Token& lhs, const Token& rhs) {
        using namespace token_type;

        if (lhs.index() != rhs.index()) {
            return false;
        }
        if (lhs.Is<Char>()) {
            return lhs.As<Char>().value == rhs.As<Char>().value;
        }
        if (lhs.Is<Number>()) {
            return lhs.As<Number>().value == rhs.As<Number>().value;
        }
        if (lhs.Is<String>()) {
            return lhs.As<String>().value == rhs.As<String>().value;
        }
        if (lhs.Is<Id>()) {
            return lhs.As<Id>().value == rhs.As<Id>().value;
        }
        return true;
    }

    bool operator!=(const Token& lhs, const Token& rhs) {
        return !(lhs == rhs);
    }

    std::ostream& operator<<(std::ostream& os, const Token& rhs) {
        using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

        VALUED_OUTPUT(Number)
        VALUED_OUTPUT(Id)
        VALUED_OUTPUT(String)
        VALUED_OUTPUT(Char)

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class)
        UNVALUED_OUTPUT(Return)
        UNVALUED_OUTPUT(If)
        UNVALUED_OUTPUT(Else)
        UNVALUED_OUTPUT(Def)
        UNVALUED_OUTPUT(Newline)
        UNVALUED_OUTPUT(Print)
        UNVALUED_OUTPUT(Indent)
        UNVALUED_OUTPUT(Dedent)
        UNVALUED_OUTPUT(And)
        UNVALUED_OUTPUT(Or)
        UNVALUED_OUTPUT(Not)
        UNVALUED_OUTPUT(Eq)
        UNVALUED_OUTPUT(NotEq)
        UNVALUED_OUTPUT(LessOrEq)
        UNVALUED_OUTPUT(GreaterOrEq)
        UNVALUED_OUTPUT(None)
        UNVALUED_OUTPUT(True)
        UNVALUED_OUTPUT(False)
        UNVALUED_OUTPUT(Eof)

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input): input_(input) { NextToken(); }

    const Token &Lexer::CurrentToken() const {
        return tokinazer_.tokens_.at(index_current_token_);
    }

    void Lexer::ParseTokens(std::istream& input){
        LineTokenizer line;
        for (line = LineTokenizer::ReadLine(input); line.IsEmpty(); line = LineTokenizer::ReadLine(input)){}
        if (line.indent_ % 2 != 0) { throw LexerError("Parsing error: indentation"); }

        if (!line.IsEofOnly() && line.indent_ > current_indent_) {
            std::vector<Token> vector_indent((line.indent_ - current_indent_) / 2, token_type::Indent{});
            tokinazer_.tokens_.insert(tokinazer_.tokens_.end(), vector_indent.begin(), vector_indent.end());
            current_indent_ = line.indent_;
        }
        if (!line.IsEofOnly() && line.indent_ < current_indent_) {
            std::vector<Token> vector_indent((current_indent_ - line.indent_) / 2, token_type::Dedent{});
            tokinazer_.tokens_.insert(tokinazer_.tokens_.end(), vector_indent.begin(), vector_indent.end());
            current_indent_ = line.indent_;
        }
        if (line.IsEofOnly() && current_indent_ > 0) {
            std::vector<Token> vector_dedent(current_indent_ / 2, token_type::Dedent{});
            tokinazer_.tokens_.insert(tokinazer_.tokens_.end(), vector_dedent.begin(), vector_dedent.end());
            current_indent_ = line.indent_;
        }
        tokinazer_.tokens_.insert(tokinazer_.tokens_.end(), line.tokens_.begin(), line.tokens_.end());
    }

    Token Lexer::NextToken() {
        // Пуст или накопленные на текущей строке токены закончились
        if (tokinazer_.tokens_.empty() || static_cast<size_t>(index_current_token_) == tokinazer_.tokens_.size() - 1) {
            tokinazer_.tokens_.clear();
            index_current_token_ = -1;
            ParseTokens(input_);
        }
        return tokinazer_.tokens_.at(++index_current_token_);
    }


    //TODO:: Переделать через getline
    Lexer::LineTokenizer Lexer::LineTokenizer::ReadLine(std::istream &input) {
        LineTokenizer line;
        line.indent_ = SkipSpaces(input);


        for (int ch = 0; !(ch == NEW_LINE_SIGN || ch == EOF); ) {
            ch = input.peek();
            switch (ch) {
                case SPACE_SIGN:
                    SkipSpaces(input);
                    break;
                case COMMENT_SIGN:
                    SkipComment(input);
                    break;
                case NEW_LINE_SIGN:
                    input.get();
                    line.tokens_.emplace_back(token_type::Newline{});
                    break;
                case EOF:
                    if (!line.IsEmpty() && !line.tokens_.back().Is<token_type::Newline>()) {
                        line.tokens_.emplace_back(token_type::Newline{});
                    }
                    line.tokens_.emplace_back(token_type::Eof{});
                    break;
                case '"':
                case '\'':
                    line.tokens_.emplace_back(ParseString(input));
                    break;
                default:
                    if (std::isdigit(ch)) {
                        line.tokens_.emplace_back(ParseNumber(input));
                    } else if (std::isalnum(ch) || ch == '_') {
                        line.ParseNameOrToken(input);
                    } else {
                        line.ParseComparisonOrChar(input);
                    }
                    break;
            }
        }
        return line;
    }

    int Lexer::LineTokenizer::SkipSpaces(std::istream &input) {
        int space_count = 0;
        for (; input.get() == SPACE_SIGN; ++space_count){}
        input.unget();
        return space_count;
    }

    void Lexer::LineTokenizer::SkipComment(std::istream &input) {
        for (; !(input.get() == NEW_LINE_SIGN || input.eof());){}
        input.unget();
    }

    token_type::String Lexer::LineTokenizer::ParseString(std::istream &input) {
        int quotation_mark = input.get();
        auto it = std::istreambuf_iterator<char>(input);
        auto end = std::istreambuf_iterator<char>();
        std::string str;
        while (true) {
            if (it == end) { throw LexerError("String parsing error"); }
            const char ch = *it;
            if (ch == quotation_mark) {
                ++it;
                break;
            } else if (ch == '\\') {
                ++it;
                if (it == end) { throw LexerError("String parsing error"); }
                const char escaped_char = *(it);
                switch (escaped_char) {
                    case 'n':
                        str.push_back('\n');
                        break;
                    case 't':
                        str.push_back('\t');
                        break;
                    case 'r':
                        str.push_back('\r');
                        break;
                    case '"':
                        str.push_back('"');
                        break;
                    case '\'':
                        str.push_back('\'');
                        break;
                    case '\\':
                        str.push_back('\\');
                        break;
                    default:
                        throw LexerError("Unrecognized escape sequence \\"s + escaped_char);
                }
            } else if (ch == '\n' || ch == '\r') {
                throw LexerError("Unexpected end of line"s);
            } else {
                str.push_back(ch);
            }
            ++it;
        }
        return token_type::String{str};
    }

    token_type::Number Lexer::LineTokenizer::ParseNumber(std::istream &input) {
        std::string str{static_cast<char>(input.get())};
        for (int ch = input.get(); input && std::isdigit(ch); ch = input.get()) {
            str += static_cast<char>(ch);
        }
        input.unget();
        return token_type::Number{stoi(str)};
    }

    void Lexer::LineTokenizer::ParseNameOrToken(std::istream &input) {
        std::string str{static_cast<char>(input.get())};
        for (int ch = input.get(); input && (std::isalnum(ch) || ch == '_'); ch = input.get()) {
            str.push_back(static_cast<char>(ch));
        }
        input.unget();
        tokens_.emplace_back(lexemes.count(str) ? lexemes.at(str) : token_type::Id{std::move(str)} );
    }

    void Lexer::LineTokenizer::ParseComparisonOrChar(std::istream &input) {
        std::string sym_pair = {static_cast<char>(input.get()), static_cast<char>(input.peek())};
        if (lexemes.count(sym_pair)) {
            tokens_.emplace_back(lexemes.at(sym_pair));
            input.get();
        } else {
            tokens_.emplace_back(token_type::Char{sym_pair[0]});
        }
    }

    bool Lexer::LineTokenizer::IsEmpty() const {
        return tokens_.empty() || std::all_of(tokens_.cbegin(), tokens_.cend(), [](const auto &t) {
            return t.template Is<token_type::Newline>();
        });
    }

    bool Lexer::LineTokenizer::IsEofOnly() const {
        return tokens_.empty() || std::all_of(tokens_.cbegin(), tokens_.cend(), [](const auto &t) {
            return t.template Is<token_type::Eof>();
        });
    }

    void Lexer::LineTokenizer::Reset(){
        tokens_.clear();
        indent_ = 0;
    }
}  // namespace parse