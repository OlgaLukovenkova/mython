#pragma once

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace runtime {

    // context of execution of Mython commands
    class Context {
    public:
        // return output strean for print
        virtual std::ostream& GetOutputStream() = 0;

    protected:
        ~Context() = default;
    };

	// base class for all Mython objects
    class Object {
    public:
        virtual ~Object() = default;
        // output into os its own string representation
        virtual void Print(std::ostream& os, Context& context) = 0;
    };

    // special class wrapper for storage object in Mython program 
	Специальный класс-обёртка, предназначенный для хранения объекта в Mython-программе
    class ObjectHolder {
    public:
        // create empty value
        ObjectHolder() = default;

        // return ObjectHolder that owns object of type T
        // T is derived class from Object.
        // object is copied or moved into heap
        template <typename T>
        [[nodiscard]] static ObjectHolder Own(T&& object) {
            return ObjectHolder(std::make_shared<T>(std::forward<T>(object)));
        }

        // create ObjectHolder that does not own object of type T (weak ref)
        [[nodiscard]] static ObjectHolder Share(Object& object);

        // create empty ObjectHolder which corresponds to None
        [[nodiscard]] static ObjectHolder None();

        // return ref to Object which is inside ObjectHolder.
        // ObjectHolder must not be empty
        Object& operator*() const;

        Object* operator->() const;

        [[nodiscard]] Object* Get() const;

        // return pointer to object of T type or nullptr, if ObjectHolder does not 
        // contain object of T type
        template <typename T>
        [[nodiscard]] T* TryAs() const {
            return dynamic_cast<T*>(this->Get());
        }

        // return true, if ObjectHolder is not empty
        explicit operator bool() const;

    private:
        explicit ObjectHolder(std::shared_ptr<Object> data);
        void AssertIsValid() const;

        std::shared_ptr<Object> data_;
    };

    // object which stores value of T type
    template <typename T>
    class ValueObject : public Object {
    public:
        ValueObject(T v)  // NOLINT(google-explicit-constructor,hicpp-explicit-conversions)
            : value_(v) {
        }

        void Print(std::ostream& os, [[maybe_unused]] Context& context) override {
            os << value_;
        }

        [[nodiscard]] const T& GetValue() const {
            return value_;
        }

    private:
        T value_;
    };

    // table of symbols which links objects' names and values
    using Closure = std::unordered_map<std::string, ObjectHolder>;

    // chek if object contains value which can be transformed into True
    // for numbers other than 0 and non-empty strings return True, in other cases return False.
    bool IsTrue(const ObjectHolder& object);

    // interface for execution different actions under Mython objects
    class Executable {
    public:
        virtual ~Executable() = default;
        // Выполняет действие над объектами внутри closure, используя context
        // Возвращает результирующее значение либо None
        virtual ObjectHolder Execute(Closure& closure, Context& context) = 0;
    };

    // String value
    using String = ValueObject<std::string>;
    // Number value
    using Number = ValueObject<int>;

    // Logical value
    class Bool : public ValueObject<bool> {
    public:
        using ValueObject<bool>::ValueObject;

        void Print(std::ostream& os, Context& context) override;
    };

    // Method of class
    struct Method {
        std::string name;
        
        std::vector<std::string> formal_params;
       
        std::unique_ptr<Executable> body;
    };


    class Class : public Object {
    public:
        // Создаёт класс с именем name и набором методов methods, унаследованный от класса parent
        // Если parent равен nullptr, то создаётся базовый класс
        explicit Class(std::string name, std::vector<Method> methods, const Class* parent);

        // Возвращает указатель на метод name или nullptr, если метод с таким именем отсутствует
        [[nodiscard]] const Method* GetMethod(const std::string& name) const;

        // Возвращает имя класса
        [[nodiscard]] const std::string& GetName() const;

        // Выводит в os строку "Class <имя класса>", например "Class cat"
        void Print(std::ostream& os, Context& context) override;
    private:
        std::string name_;
        std::unordered_map<std::string_view, std::shared_ptr<Method>> vtbl_;
        const Class* parent_;
    };

    class ClassInstance : public Object {
    public:
        explicit ClassInstance(const Class& cls);

        /*
         * If object has __str__ method then output its result into os 
         * Else output object's adress
         */
        void Print(std::ostream& os, Context& context) override;

        /*
         * Call object's method
         * If class or its base classes do not have such method throw exception runtime_error
         */
        ObjectHolder Call(const std::string& method, const std::vector<ObjectHolder>& actual_args,
            Context& context);

        // return true if object has method with argument_count parameters
        [[nodiscard]] bool HasMethod(const std::string& method, size_t argument_count) const;

        // return ref to Closure which contains attributes of object
        [[nodiscard]] Closure& Fields();
        // return const ref to Closure which contains attributes of object
        [[nodiscard]] const Closure& Fields() const;
    private:
        Closure fields_;
        const Class& class_;
    };

    /*
     * return true if lhs and rhs contain equal numbers, strings or Bool values.
     * If lhs is object and it has method __eq__ then function returns result of lhs.__eq__(rhs)
     * which is transformed into Bool type. If lhs and rhs are both None function returns true.
     * Else throws exception runtime_error.
     */
    bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    /*
     * If lhs and rhs are numbers, strings or Bool values, return result of their comparison by operator <.
     * If lhs is object and it has method __lt__ then function returns result of  lhs.__lt__(rhs),
     * which is transformed into Bool type. Else throws exception runtime_error.
     */
    bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // return value opposite to Equal(lhs, rhs, context)
    bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // return lhs>rhs using Equal and Less
    bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // return lhs<=rhs using Equal and Less
    bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);
    // return value opposite to Less(lhs, rhs, context)
    bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context);

    // Context for tests
    struct DummyContext : Context {
        std::ostream& GetOutputStream() override {
            return output;
        }

        std::ostringstream output;
    };

    // Simple context
    class SimpleContext : public runtime::Context {
    public:
        explicit SimpleContext(std::ostream& output)
            : output_(output) {
        }

        std::ostream& GetOutputStream() override {
            return output_;
        }

    private:
        std::ostream& output_;
    };

}  // namespace runtime