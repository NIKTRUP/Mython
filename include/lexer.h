#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <unordered_map>
#include <functional>

namespace parse {

    namespace token_type {
        struct Number {  // Лексема «число»
            int value;   // число
        };

        struct Id {             // Лексема «идентификатор»
            std::string value;  // Имя идентификатора
        };

        struct Char {    // Лексема «символ»
            char value;  // код символа
        };

        struct String {  // Лексема «строковая константа»
            std::string value;
        };

        struct Class {};    // Лексема «class»
        struct Return {};   // Лексема «return»
        struct If {};       // Лексема «if»
        struct Else {};     // Лексема «else»
        struct Def {};      // Лексема «def»
        struct Newline {};  // Лексема «конец строки»
        struct Print {};    // Лексема «print»
        struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
        struct Dedent {};  // Лексема «уменьшение отступа»
        struct Eof {};     // Лексема «конец файла»
        struct And {};     // Лексема «and»
        struct Or {};      // Лексема «or»
        struct Not {};     // Лексема «not»
        struct Eq {};      // Лексема «==»
        struct NotEq {};   // Лексема «!=»
        struct LessOrEq {};     // Лексема «<=»
        struct GreaterOrEq {};  // Лексема «>=»
        struct None {};         // Лексема «None»
        struct True {};         // Лексема «True»
        struct False {};        // Лексема «False»
    }  // namespace token_type

    using TokenBase
            = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
            token_type::Class, token_type::Return, token_type::If, token_type::Else,
            token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
            token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
            token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
            token_type::None, token_type::True, token_type::False, token_type::Eof>;

    struct Token : TokenBase {
        using TokenBase::TokenBase;

        template <typename T>
        [[nodiscard]] bool Is() const {
            return std::holds_alternative<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T& As() const {
            return std::get<T>(*this);
        }

        template <typename T>
        [[nodiscard]] const T* TryAs() const {
            return std::get_if<T>(this);
        }
    };

    bool operator==(const Token& lhs, const Token& rhs);
    bool operator!=(const Token& lhsisspace, const Token& rhs);

    std::ostream& operator<<(std::ostream& os, const Token& rhs);

    class LexerError : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    static const std::unordered_map<std::string, parse::Token> lexemes = {
            {"class",   parse::token_type::Class{}},
            {"return",  parse::token_type::Return{}},
            {"if",      parse::token_type::If{}},
            {"else",    parse::token_type::Else{}},
            {"def",     parse::token_type::Def{}},
            {"print",   parse::token_type::Print{}},
            {"and",     parse::token_type::And{}},
            {"or",      parse::token_type::Or{}},
            {"not",     parse::token_type::Not{}},
            {"==",  parse::token_type::Eq{}},
            {"!=",  parse::token_type::NotEq{}},
            {"<=",  parse::token_type::LessOrEq{}},
            {">=",  parse::token_type::GreaterOrEq{}},
            {"None",    parse::token_type::None{}},
            {"True",    parse::token_type::True{}},
            {"False",   parse::token_type::False{}}
    };

    inline constexpr char COMMENT_SIGN = '#';
    inline constexpr char SPACE_SIGN = ' ';
    inline constexpr char NEW_LINE_SIGN = '\n';


    class Lexer {
    public:
        explicit Lexer(std::istream& input);

        // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
        [[nodiscard]] const Token &CurrentToken() const;

        // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
        Token NextToken();

        // Если текущий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template<typename T>
        const T &Expect() const {
            using namespace std::literals;
            if (!CurrentToken().Is<T>()) { throw LexerError("Not the expected type of the current token"s); }
            return CurrentToken().As<T>();
        }

        // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template<typename T, typename U>
        void Expect(const U &value) const {
            using namespace std::literals;
            if (!CurrentToken().Is<T>() || CurrentToken().As<T>().value != value) {
                throw LexerError("Not the expected type of the current token or value"s);
            }
        }

        // Если следующий токен имеет тип T, метод возвращает ссылку на него.
        // В противном случае метод выбрасывает исключение LexerError
        template<typename T>
        const T &ExpectNext() {
            using namespace std::literals;
            NextToken();
            return Expect<T>();
        }

        // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
        // В противном случае метод выбрасывает исключение LexerError
        template<typename T, typename U>
        void ExpectNext(const U &value) {
            using namespace std::literals;
            NextToken();
            Expect<T>(value);
        }

    private:
        void ParseTokens(std::istream& input);

        struct LineTokenizer {
            static Lexer::LineTokenizer ReadLine(std::istream &input);

            static int SkipSpaces(std::istream &input);

            static void SkipComment(std::istream &input);

            static token_type::String ParseString(std::istream &input);

            static token_type::Number ParseNumber(std::istream &input);

            void ParseNameOrToken(std::istream &input);

            void ParseComparisonOrChar(std::istream &input);

            [[nodiscard]] bool IsEmpty() const;

            [[nodiscard]] bool IsEofOnly() const;

            void Reset();

            int indent_ = 0;
            std::vector<Token> tokens_;
        } tokinazer_;

        std::istream& input_;
        int current_indent_ = 0;
        int index_current_token_ = -1;
    };

}  // namespace parse