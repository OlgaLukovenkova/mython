#pragma once

#include "runtime.h"

#include <functional>

namespace ast {

    using Statement = runtime::Executable;

    // base for constants of T type
    template <typename T>
    class ValueStatement : public Statement {
    public:
        explicit ValueStatement(T v)
            : value_(std::move(v)) {
        }

        runtime::ObjectHolder Execute(runtime::Closure& /*closure*/,
            runtime::Context& /*context*/) override {
            return runtime::ObjectHolder::Share(value_);
        }

    private:
        T value_;

    };

    using NumericConst = ValueStatement<runtime::Number>;
    using StringConst = ValueStatement<runtime::String>;
    using BoolConst = ValueStatement<runtime::Bool>;

    /*
    calculates value of variable with certain name or chain of field names of object id1.id2.id3.
    */
    class VariableValue : public Statement {
    public:
        explicit VariableValue(const std::string& var_name);
        explicit VariableValue(std::vector<std::string> dotted_ids);
        VariableValue(const VariableValue&) = default;
        VariableValue(VariableValue&&) = default;

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    private:
        std::vector<std::string> var_name_chain_;
        static runtime::ObjectHolder FindVariable(runtime::Closure& closure, const std::string& name);
    };

    // assign variable (which name is var) value of statement rv
    class Assignment : public Statement {
    public:
        Assignment(std::string var, std::unique_ptr<Statement> rv);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

    private:
        std::string var_name_;
        std::unique_ptr<Statement> value_;
    };

    // assign field object.field_name value of statement rv
    class FieldAssignment : public Statement {
    public:
        FieldAssignment(VariableValue object, std::string field_name, std::unique_ptr<Statement> rv);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    private:
        VariableValue obj_;
        std::string field_name_;
        std::unique_ptr<Statement> value_;
    };

    // value None
    class None : public Statement {
    public:
        runtime::ObjectHolder Execute([[maybe_unused]] runtime::Closure& closure,
            [[maybe_unused]] runtime::Context& context) override {
            return {};
        }
    };

    // command print
    class Print : public Statement {
    public:
        explicit Print(std::unique_ptr<Statement> argument);
        explicit Print(std::vector<std::unique_ptr<Statement>> args);
        static std::unique_ptr<Print> Variable(const std::string& name);

        // output to stream which is result of context.GetOutputStream()
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    private:
        std::vector<std::unique_ptr<Statement>> args_;
    };

    // call object.method with parameter list of args
    class MethodCall : public Statement {
    public:
        MethodCall(std::unique_ptr<Statement> object, std::string method,
            std::vector<std::unique_ptr<Statement>> args);
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    
    private:
        std::unique_ptr<Statement> obj_;
        std::string method_name_;
        std::vector<std::unique_ptr<Statement>> args_;
    };

    /*
    creates new instance of class_ trying call __init__ with parameters args.
    If method __init__ with specified number of parameters does not exist 
    then instance creates without __init__ call (fields are not initialised)
    */
    class NewInstance : public Statement {
    public:
        explicit NewInstance(const runtime::Class& class_);
        NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args);
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
		
    private:
        const runtime::Class& class_;
        std::vector<std::unique_ptr<Statement>> args_;
    };

    class UnaryOperation : public Statement {
    public:
        explicit UnaryOperation(std::unique_ptr<Statement> argument) 
            : arg_(std::move(argument)) 
        {
        }

    protected:
        std::unique_ptr<Statement> arg_;
    };

    // command str()
    class Stringify : public UnaryOperation {
    public:
        using UnaryOperation::UnaryOperation;
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    class BinaryOperation : public Statement {
    public:
        BinaryOperation(std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs) 
            : lhs_(std::move(lhs)), rhs_(std::move(rhs)) 
        {
        
        }

    protected:
        std::unique_ptr<Statement> lhs_;
        std::unique_ptr<Statement> rhs_;    
    };

    // lhs + rhs
    class Add : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Support sum
		// number + number
		// string + string
		// obj1 + obj2 if obj1 has __add__ method
        // else throws runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // lhs - rhs
    class Sub : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Support subtraction
		// number - number
        // else throw runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // lhs * rhs
    class Mult : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Support multiplication
		// number * number
        // else throw runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // lhs / rhs
    class Div : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;

        // Support division
		// number / number
        // else throw runtime_error
        // if rhs = 0 throw runtime_error
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // lhs or rhs
    class Or : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;
        // value of rhs calculates only if lhs transformed into Bool is equal to False
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // lhs and rhs
    class And : public BinaryOperation {
    public:
        using BinaryOperation::BinaryOperation;
        // value of rhs calculates only if lhs transformed into Bool is equal to True
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    class Not : public UnaryOperation {
    public:
        using UnaryOperation::UnaryOperation;
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    };

    // several commands (for example, method bodey, code of if- or else- branches)
    class Compound : public Statement {
    public:
        
        template <typename... Args>
        explicit Compound(Args&&... args) {
            (commands_.push_back(std::move(args)), ...);
        }
		
		// add new statement to the end of list
        void AddStatement(std::unique_ptr<Statement> stmt) {
            commands_.push_back(std::move(stmt));
        }

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

    private:
        std::vector<std::unique_ptr<Statement>> commands_;
    };

    class MethodBody : public Statement {
    public:
        explicit MethodBody(std::unique_ptr<Statement>&& body);

        // if inside body return command was ewecuted then return the result of return command
        // else return None
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    private:
        std::unique_ptr<Statement> body_;
    };

    class Return : public Statement {
    public:
        explicit Return(std::unique_ptr<Statement> statement) 
            : statement_(std::move(statement)) 
        {

        }

        // stop execution of current method, it shoud return result of calculation of return statement
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    private:
        std::unique_ptr<Statement> statement_;
    };

    class ClassDefinition : public Statement {
    public:
        explicit ClassDefinition(runtime::ObjectHolder cls);

        // creates inside closure new object with name of class and the value which was passed into constructor
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

    private:
        runtime::ObjectHolder class_;
    };

    // if <condition> <if_body> else <else_body>
    class IfElse : public Statement {
    public:
        // else_body can be nullptr
        IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
            std::unique_ptr<Statement> else_body);

        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;

    private:
        std::unique_ptr<Statement> condition_;
        std::unique_ptr<Statement> if_body_;
        std::unique_ptr<Statement> else_body_;
    };

    class Comparison : public BinaryOperation {
    public:
        // function defining comparison
        using Comparator = std::function<bool(const runtime::ObjectHolder&,
            const runtime::ObjectHolder&, runtime::Context&)>;

        Comparison(Comparator cmp, std::unique_ptr<Statement> lhs, std::unique_ptr<Statement> rhs);

        // calculate lhs and rhs, return result of comparator(lhs, rhs, context) 
        // transformed into runtime::Bool
        runtime::ObjectHolder Execute(runtime::Closure& closure, runtime::Context& context) override;
    private:
        Comparator cmp_;
    };

}  // namespace ast