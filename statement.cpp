#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

    using runtime::Closure;
    using runtime::Context;
    using runtime::ObjectHolder;

    namespace {
        const string ADD_METHOD = "__add__"s;
        const string INIT_METHOD = "__init__"s;
        const string STR_METHOD = "__str__"s;
    }  // namespace

    /*Assignment*/
    ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
        closure[var_name_] = value_->Execute(closure, context);
        return closure.at(var_name_);
    }

    Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv) :
        var_name_(std::move(var)), value_(std::move(rv)) 
    {

    }

    /*Variable value*/
    VariableValue::VariableValue(const std::string& var_name) 
        : var_name_chain_{ var_name } 
    {

    }

    VariableValue::VariableValue(std::vector<std::string> dotted_ids) 
        : var_name_chain_{ std::move(dotted_ids) } 
    {

    }

    ObjectHolder VariableValue::FindVariable(Closure& closure, const std::string& name) {
        if (closure.count(name) == 0) {
            throw std::runtime_error("Unknown variable"s);
        }
        return closure.at(name);
    }

    ObjectHolder VariableValue::Execute(Closure& closure, Context&) {
        ObjectHolder obj = VariableValue::FindVariable(closure, var_name_chain_[0]);
        if (var_name_chain_.size() == 1) {
            return obj;
        }

        for (size_t i = 1; i < var_name_chain_.size(); ++i) {
            runtime::ClassInstance* class_ins_ptr = obj.TryAs<runtime::ClassInstance>();
            if (!class_ins_ptr) {
                throw std::runtime_error("Wrong type"s);
            }
            Closure& fields_closure = class_ins_ptr->Fields();
            obj = VariableValue::FindVariable(fields_closure, var_name_chain_[i]);
        }
        return obj;
    }

    /*Print*/
    unique_ptr<Print> Print::Variable(const std::string& name) {
        unique_ptr<Statement> ptr(new VariableValue(name));
        return std::make_unique<Print>(std::move(ptr));
    }

    Print::Print(unique_ptr<Statement> argument) {
        args_.push_back(std::move(argument));
    }

    Print::Print(vector<unique_ptr<Statement>> args) 
        : args_(std::move(args))
    {
    }

    ObjectHolder Print::Execute(Closure& closure, Context& context) {
        auto& os = context.GetOutputStream();
        bool is_not_first = false;
        for (auto& arg_ptr : args_) {
            if (is_not_first) {
                os << " ";
            }
            if (ObjectHolder holder = arg_ptr->Execute(closure, context); holder) {
                holder->Print(os, context);
            }
            else {
                os << "None";
            }
            is_not_first = true;
        }
        os << "\n";
        return {};
    }

    
    /*Method call*/
    MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
        std::vector<std::unique_ptr<Statement>> args) 
        : obj_(std::move(object))
        , method_name_(std::move(method))
        , args_(std::move(args)) 
    {

    }

    ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
        std::vector<ObjectHolder> arg_values;
        for (auto& ptr : args_) {
            arg_values.push_back(ptr->Execute(closure, context));
        }
        ObjectHolder holder = obj_->Execute(closure, context);
        if (auto obj = holder.TryAs<runtime::ClassInstance>(); 
            obj && obj->HasMethod(method_name_, arg_values.size())) {
            return obj->Call(method_name_, arg_values, context);
        }
        throw std::runtime_error("Wrong method call"s);
    }


    /*Transformation into string*/
    ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
        ObjectHolder holder = arg_->Execute(closure, context);
        runtime::Object* res;
        if (auto obj = holder.TryAs<runtime::ClassInstance>();
            obj && obj->HasMethod(STR_METHOD, 0)) {
            res = obj->Call(STR_METHOD, {}, context).Get();
        }
        else {
            res = holder.Get();
        }
        std::ostringstream os;
        if (res) {
            res->Print(os, context);
        }
        else {
            os << "None";
        }
        return ObjectHolder::Own(std::move(runtime::String{os.str()}));
    }


    /*Different arithmetic operations*/
    ObjectHolder Add::Execute(Closure& closure, Context& context) {
        ObjectHolder left = lhs_->Execute(closure, context);
        ObjectHolder right = rhs_->Execute(closure, context);

        if (left.TryAs<runtime::Number>() && right.TryAs<runtime::Number>()) {
            int res = left.TryAs<runtime::Number>()->GetValue() + right.TryAs<runtime::Number>()->GetValue();
            return ObjectHolder::Own(runtime::Number(res));
        }

        if (left.TryAs<runtime::String>() && right.TryAs<runtime::String>()) {
            std::string res = left.TryAs<runtime::String>()->GetValue() + right.TryAs<runtime::String>()->GetValue();
            return ObjectHolder::Own(runtime::String(res));
        }

        if (runtime::ClassInstance* obj_ptr = left.TryAs<runtime::ClassInstance>();
            obj_ptr && obj_ptr->HasMethod(ADD_METHOD, 1)) {
            return obj_ptr->Call(ADD_METHOD, { right }, context);
        }

        throw std::runtime_error("ADD is unavailable"s);
    }

    ObjectHolder Sub::Execute(Closure& closure, Context& context) {
        ObjectHolder left = lhs_->Execute(closure, context);
        ObjectHolder right = rhs_->Execute(closure, context);

        if (left.TryAs<runtime::Number>() && right.TryAs<runtime::Number>()) {
            int res = left.TryAs<runtime::Number>()->GetValue() - right.TryAs<runtime::Number>()->GetValue();
            return ObjectHolder::Own(runtime::Number(res));
        }

        throw std::runtime_error("SUB is unavailable"s);
    }

    ObjectHolder Mult::Execute(Closure& closure, Context& context) {
        ObjectHolder left = lhs_->Execute(closure, context);
        ObjectHolder right = rhs_->Execute(closure, context);

        if (left.TryAs<runtime::Number>() && right.TryAs<runtime::Number>()) {
            int res = left.TryAs<runtime::Number>()->GetValue() * right.TryAs<runtime::Number>()->GetValue();
            return ObjectHolder::Own(runtime::Number(res));
        }

        throw std::runtime_error("MULT is unavailable"s);
    }

    ObjectHolder Div::Execute(Closure& closure, Context& context) {
        ObjectHolder left = lhs_->Execute(closure, context);
        ObjectHolder right = rhs_->Execute(closure, context);

        if (left.TryAs<runtime::Number>() && right.TryAs<runtime::Number>()) {
            if (int den = right.TryAs<runtime::Number>()->GetValue(); den != 0) {
                int res = left.TryAs<runtime::Number>()->GetValue() / right.TryAs<runtime::Number>()->GetValue();
                return ObjectHolder::Own(runtime::Number(res));
            }
            else {
                throw std::runtime_error("Denominator is 0"s);
            }
        }

        throw std::runtime_error("MULT is unavailable"s);
    }


    /*Block of several commands*/
    ObjectHolder Compound::Execute(Closure& closure, Context& context) {
        for (auto& ptr : commands_) {
            ObjectHolder holder = ptr->Execute(closure, context);
            if (dynamic_cast<Return*>(ptr.get()) || 
                (dynamic_cast<IfElse*>(ptr.get()) && holder) ||
                (dynamic_cast<Compound*>(ptr.get()) && holder) ) {
                return holder;
            }
        }
        return {};
    }


    /*Return statement*/
    ObjectHolder Return::Execute(Closure& closure, Context& context) {
        return statement_->Execute(closure, context);
    }


    /*Class definition*/
    ClassDefinition::ClassDefinition(ObjectHolder cls) 
        : class_(std::move(cls)) 
    {

    }

    ObjectHolder ClassDefinition::Execute(Closure& closure, Context&) {
        closure[class_.TryAs<runtime::Class>()->GetName()] = std::move(class_);
        return {};
    }
    

    /*Assignment value to class field*/
    FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
        std::unique_ptr<Statement> rv) 
        : obj_(std::move(object))
        , field_name_(std::move(field_name))
        , value_(std::move(rv))
    {

    }

    ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
        runtime::ClassInstance* ins_ptr = obj_.Execute(closure, context).TryAs<runtime::ClassInstance>();
        if (!ins_ptr) {
            throw std::runtime_error("Object is not a class instance"s);
        }
        ins_ptr->Fields()[field_name_] = std::move(value_->Execute(closure, context));
        return ins_ptr->Fields().at(field_name_);
    }


    /*If - else - block*/
    IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
        std::unique_ptr<Statement> else_body) 
        : condition_(std::move(condition))
        , if_body_(std::move(if_body))
        , else_body_(std::move(else_body))
    {

    }

    ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
        if (IsTrue(condition_->Execute(closure, context))) {
            return if_body_->Execute(closure, context);
        }
        else {
            if (else_body_) {
                return else_body_->Execute(closure, context);
            }
            else {
                return {};
            }
        }
    }


    /*Different logic operations*/
    ObjectHolder Or::Execute(Closure& closure, Context& context) {
        ObjectHolder left = lhs_->Execute(closure, context);
        ObjectHolder right = rhs_->Execute(closure, context);

        if (runtime::IsTrue(left)) {
            return ObjectHolder::Own(runtime::Bool(true));
        }
        else {
            if (runtime::IsTrue(right)) {
                return ObjectHolder::Own(runtime::Bool(true));
            }
        }

        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder And::Execute(Closure& closure, Context& context) {
        ObjectHolder left = lhs_->Execute(closure, context);
        ObjectHolder right = rhs_->Execute(closure, context);

        if (!runtime::IsTrue(left)) {
            return ObjectHolder::Own(runtime::Bool(false));
        }
        else {
            if (!runtime::IsTrue(right)) {
                return ObjectHolder::Own(runtime::Bool(false));
            }
        }

        return ObjectHolder::Own(runtime::Bool(true));
    }

    ObjectHolder Not::Execute(Closure& closure, Context& context) {
        ObjectHolder arg = arg_->Execute(closure, context);

        if (runtime::IsTrue(arg)) {
            return ObjectHolder::Own(runtime::Bool(false));
        }

        return ObjectHolder::Own(runtime::Bool(true));
    }


    /*Comparison*/
    Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
        : BinaryOperation(std::move(lhs), std::move(rhs))
        , cmp_(std::move(cmp))
    {

    }

    ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
        ObjectHolder left = lhs_->Execute(closure, context);
        ObjectHolder right = rhs_->Execute(closure, context);

        return ObjectHolder::Own(runtime::Bool(cmp_(left, right, context)));
    }


    /*New object of some class*/
    NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args) 
        : class_(class_)
        , args_(std::move(args))
    {

    }

    NewInstance::NewInstance(const runtime::Class& class_) 
        : NewInstance(class_, {})
    {

    }

    ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
        ObjectHolder new_obj = ObjectHolder::Own(runtime::ClassInstance(class_));
        
        if (const runtime::Method* init_ptr = class_.GetMethod(INIT_METHOD);
            init_ptr && init_ptr->formal_params.size() == args_.size()) {
            std::vector<ObjectHolder> args;
            for (auto& arg : args_) {
                args.push_back(arg->Execute(closure, context));
            }
            new_obj.TryAs<runtime::ClassInstance>()->Call(INIT_METHOD,  args, context); 
        }
       
        return new_obj;
    }


    /*Method body*/
    MethodBody::MethodBody(std::unique_ptr<Statement>&& body) 
        : body_(std::move(body))
    {

    }

    ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
        return body_->Execute(closure, context);
    }

}  // namespace ast