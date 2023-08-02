#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <iostream>

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

        VALUED_OUTPUT(Number);
        VALUED_OUTPUT(Id);
        VALUED_OUTPUT(String);
        VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

        UNVALUED_OUTPUT(Class);
        UNVALUED_OUTPUT(Return);
        UNVALUED_OUTPUT(If);
        UNVALUED_OUTPUT(Else);
        UNVALUED_OUTPUT(Def);
        UNVALUED_OUTPUT(Newline);
        UNVALUED_OUTPUT(Print);
        UNVALUED_OUTPUT(Indent);
        UNVALUED_OUTPUT(Dedent);
        UNVALUED_OUTPUT(And);
        UNVALUED_OUTPUT(Or);
        UNVALUED_OUTPUT(Not);
        UNVALUED_OUTPUT(Eq);
        UNVALUED_OUTPUT(NotEq);
        UNVALUED_OUTPUT(LessOrEq);
        UNVALUED_OUTPUT(GreaterOrEq);
        UNVALUED_OUTPUT(None);
        UNVALUED_OUTPUT(True);
        UNVALUED_OUTPUT(False);
        UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

        return os << "Unknown token :("sv;
    }

    Lexer::Lexer(std::istream& input)
        : in_(input) {
        ReadSpaces();
        NextToken();
    }

    const Token& Lexer::CurrentToken() const {
        return current_token_;
    }

    Token Lexer::NextToken() {
        // комментарий
        if (in_.peek() == '#') {
            std::string comment;
            getline(in_, comment);
            in_.putback('\n');
        }
        // новая строка
        if (in_.peek() == '\n') {
            in_.get();
            ReadSpaces();
            if (current_token_ != token_type::Newline()) {
                return current_token_ = token_type::Newline();
            }
            else {
                return NextToken();
            }
        }
        // отступы
        if (current_token_ != token_type::Newline() ||
            current_token_ != token_type::Indent() ||
            current_token_ != token_type::Dedent())
            if (spaces_in_str_begin > str_indent_) {
                str_indent_ += 2;
                return current_token_ = token_type::Indent();
            }
        if (spaces_in_str_begin < str_indent_) {
            str_indent_ -= 2;
            return current_token_ = token_type::Dedent();
        }
        // конец
        if (!in_) {
            if (current_token_ != token_type::Newline() &&
                current_token_ != token_type::Eof() &&
                current_token_ != token_type::Dedent()) {
                return current_token_ = token_type::Newline();
            }
            return current_token_ = token_type::Eof();
        }
        // число
        if (std::isdigit(in_.peek())) {
            return current_token_ = ReadNumber();
        }
        // строка
        if (in_.peek() == '\'' || in_.peek() == '\"') {
            char quot = in_.get();
            return current_token_ = ReadString(quot);
        }
        // идентификатор, ключевое слово
        if (in_.peek() == '_' || std::isalpha(in_.peek())) {
            return current_token_ = ReadIdentifier();
        }
        //оператор сравнения, присваивания
        if ("!=<>"s.find(in_.peek()) != std::string::npos) {
            return current_token_ = ReadComparison();
        }
        // операторы +-*/:().,
        if ("+-*/:().,"s.find(in_.peek()) != std::string::npos) {
            return current_token_ = token_type::Char{ static_cast<char>(in_.get()) };
        }

        // просто пробелы в строке
        while (in_.peek() == ' ') {
            in_.get();
        }

        return NextToken();
    }

    /*PRIVATE*/
    Token Lexer::ReadNumber() {
        int res = 0;
        while (std::isdigit(in_.peek())) {
            res *= 10;
            res += in_.get() - '0';
        }
        return token_type::Number{ res };
    }

    Token Lexer::ReadString(char quot) {
        std::string line;

        while (true) {

            if (!in_ || in_.peek() == '\n' || in_.peek() == '\r') {
                throw LexerError("String parsing error"s);
            }

            if (in_.peek() == '\\') { // для экранируемых последовательностей
                in_.get();

                switch (static_cast<char>(in_.get())) {
                case 'n':
                    line.push_back('\n');
                    break;
                case 't':
                    line.push_back('\t');
                    break;
                case '\'':
                    line.push_back('\'');
                    break;
                case '\"':
                    line.push_back('\"');
                    break;
                }
            }
            else {
                if (in_.peek() == quot) {
                    in_.get();
                    break;
                }

                line.push_back(static_cast<char>(in_.get()));
            }
        }

        return token_type::String{ move(line) };
    }

    Token Lexer::ReadIdentifier() {
        std::string line;

        while (std::isalnum(in_.peek()) || in_.peek() == '_') {
            line.push_back(static_cast<char>(in_.get()));
        }

        if (line == "class"s) {
            return token_type::Class();
        }

        if (line == "return"s) {
            return token_type::Return();
        }

        if (line == "if"s) {
            return token_type::If();
        }

        if (line == "else"s) {
            return token_type::Else();
        }

        if (line == "def"s) {
            return token_type::Def();
        }

        if (line == "print"s) {
            return token_type::Print();
        }

        if (line == "and"s) {
            return token_type::And();
        }

        if (line == "or"s) {
            return token_type::Or();
        }

        if (line == "not"s) {
            return token_type::Not();
        }

        if (line == "True"s) {
            return token_type::True();
        }

        if (line == "False"s) {
            return token_type::False();
        }

        if (line == "None"s) {
            return token_type::None();
        }

        return token_type::Id{std::move(line)};

    }

    Token Lexer::ReadComparison() {
        std::string line;
        line.push_back(static_cast<char>(in_.get()));
        if (in_.peek() == '=') {
            line.push_back(static_cast<char>(in_.get()));
        }

        if (line == "=="s) {
            return token_type::Eq();
        }

        if (line == "!="s) {
            return token_type::NotEq();
        }

        if (line == "<="s) {
            return token_type::LessOrEq();
        }

        if (line == ">="s) {
            return token_type::GreaterOrEq();
        }

        if (line == "="s || line == "<"s || line == ">"s) {
            return token_type::Char{ line[0] };
        }

        throw LexerError("Operator parsing error"s);
    }

    void Lexer::ReadSpaces() {
        spaces_in_str_begin = 0;
        while (in_.peek() == ' ') {
            in_.get();
            ++spaces_in_str_begin;
        }
        if (spaces_in_str_begin % 2 == 1) {
            throw  LexerError("Indent parsing error"s);
        }
    }

}  // namespace parse