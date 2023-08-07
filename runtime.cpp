#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>
#include <iostream>

using namespace std;

namespace runtime {

    /* --- ObjectHolder --- */
    ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
        : data_(std::move(data)) {
    }

    void ObjectHolder::AssertIsValid() const {
        assert(data_ != nullptr);
    }

    ObjectHolder ObjectHolder::Share(Object& object) {
		// return shared_ptr that does not own object (its deleted does nothing)
        return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
    }

    ObjectHolder ObjectHolder::None() {
        return ObjectHolder();
    }

    Object& ObjectHolder::operator*() const {
        AssertIsValid();
        return *Get();
    }

    Object* ObjectHolder::operator->() const {
        AssertIsValid();
        return Get();
    }

    Object* ObjectHolder::Get() const {
        return data_.get();
    }

    ObjectHolder::operator bool() const {
        return Get() != nullptr;
    }

    /* --- IsTrue() --- */
    bool IsTrue(const ObjectHolder& object) {

        if (Bool* bool_ptr = object.TryAs<Bool>()) {
            return bool_ptr->GetValue();
        }

        if (Number* number_ptr = object.TryAs<Number>()) {
            return number_ptr->GetValue() != 0;
        }

        if (String* str_ptr = object.TryAs<String>()) {
            return !str_ptr->GetValue().empty();
        }

        return false;
    }

    /* --- ClassInstance --- */
    void ClassInstance::Print(std::ostream& os, Context& context) {
        if (!HasMethod("__str__", 0)) {
            os << this;
            return;
        }
        ObjectHolder str = Call("__str__", {}, context);
        os << str.TryAs<String>()->GetValue();
    }

    bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
        const Method* method_ptr = class_.GetMethod(method);
        if (!method_ptr || method_ptr->formal_params.size() != argument_count) {
            return false;
        }
        return true;
    }

    Closure& ClassInstance::Fields() {
        return fields_;
    }

    const Closure& ClassInstance::Fields() const {
        return fields_;
    }

    ClassInstance::ClassInstance(const Class& cls)
        : class_(cls) {

    }

    ObjectHolder ClassInstance::Call(const std::string& method,
        const std::vector<ObjectHolder>& actual_args,
        Context& context) {

        if (!HasMethod(method, actual_args.size())) {
            throw runtime_error("Method can not be found"s);
        }

        const Method* method_ptr = class_.GetMethod(method);

        Closure closure;
        closure["self"s] = ObjectHolder::Share(*this);

        auto par = actual_args.begin();
        for (const std::string& par_name : method_ptr->formal_params) {
            closure[par_name] = *par;
            ++par;
        }
        return method_ptr->body->Execute(closure, context);
    }

    /* --- Class --- */
    Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
        : name_(std::move(name)), parent_(parent) {
        if (parent_) {
            vtbl_ = parent_->vtbl_;
        }
        for (Method& method : methods) {
            std::shared_ptr<Method> method_ptr = std::make_shared<Method>(std::move(method));
            vtbl_[method_ptr->name] = std::move(method_ptr);
        }
    }

    const Method* Class::GetMethod(const std::string& name) const {
        if (vtbl_.count(name) > 0) {
            return vtbl_.at(name).get();
        }
        return nullptr;
    }

    [[nodiscard]] const std::string& Class::GetName() const {
        return name_;
    }

    void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
        os << "Class "sv << name_;
    }

    /* --- Bool --- */
    void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
        os << (GetValue() ? "True"sv : "False"sv);
    }

    /* --- Comparisons --- */
    namespace {
        template <typename T, typename Func>
        std::optional<bool> CheckFunc(const ObjectHolder& lhs, const ObjectHolder& rhs, Func func) {
            if (lhs.TryAs<T>() && rhs.TryAs<T>()) {
                auto left = lhs.TryAs<T>()->GetValue();
                auto right = rhs.TryAs<T>()->GetValue();
                return func(left, right);
            }
            return std::nullopt;
        }

        template <typename T>
        bool SimpleEqual(T x, T y) {
            return x == y;
        }

        template <typename T>
        bool SimpleLess(T x, T y) {
            return x < y;
        }
    }

    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (!lhs && !rhs) {
            return true;
        }

        if (auto res = CheckFunc<Bool>(lhs, rhs, SimpleEqual<bool>)) {
            return *res;
        }

        if (auto res = CheckFunc<Number>(lhs, rhs, SimpleEqual<int>)) {
            return *res;
        }

        if (auto res = CheckFunc<String>(lhs, rhs, SimpleEqual<std::string_view>)) {
            return *res;
        }

        if (auto ptr = lhs.TryAs<ClassInstance>()) {
            if (ptr->HasMethod("__eq__"s, 1)) {
                ObjectHolder res = ptr->Call("__eq__"s, { rhs }, context);
                return res.TryAs<Bool>()->GetValue();
            }
        }

        throw std::runtime_error("Objects cannot be compared"s);
    }

    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        if (auto res = CheckFunc<Bool>(lhs, rhs, SimpleLess<bool>)) {
            return *res;
        }

        if (auto res = CheckFunc<Number>(lhs, rhs, SimpleLess<int>)) {
            return *res;
        }

        if (auto res = CheckFunc<String>(lhs, rhs, SimpleLess<std::string_view>)) {
            return *res;
        }

        if (auto ptr = lhs.TryAs<ClassInstance>()) {
            if (ptr->HasMethod("__lt__"s, 1)) {
                ObjectHolder res = ptr->Call("__lt__"s, { rhs }, context);
                return res.TryAs<Bool>()->GetValue();
            }
        }

        throw runtime_error("Objects cannot be compared"s);
    }

    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Equal(lhs, rhs, context);
    }

    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !(Less(lhs, rhs, context) || Equal(lhs, rhs, context));
    }

    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
    }

    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
        return !Less(lhs, rhs, context);
    }

}  // namespace runtime