#pragma once
#include <algorithm>
#include <optional>
#include <sstream>
#include <typeinfo>

#include "file.hpp"
#include "utils.hpp"

namespace ts {

// TODO: May be compiler dependent
template <typename T>
constexpr std::string_view type_name() {
    constexpr auto prefix = std::string_view{"[with T = "};
    constexpr auto suffix = std::string_view{";"};
    constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
    constexpr auto start = function.find(prefix) + prefix.size();
    constexpr auto end = function.rfind(suffix);
    static_assert(start < end);
    constexpr auto result = function.substr(start, (end - start));
    return result;
}

class Class {
protected:
    std::string name_;

    void Validate() {
        for (auto& c : name_) {
            if (c == '@' || c == '_' || c == '<' || c == '>') {
                throw error::TypeError("Invalid class name");
            }
        }
    }

public:
    explicit Class(std::string name) : name_(std::move(name)) {
        Validate();
    }
    virtual ~Class() = default;

    [[nodiscard]] virtual std::string Serialize() const = 0;

    [[nodiscard]] virtual std::optional<size_t> Size() const = 0;

    [[nodiscard]] virtual std::string Name() const {
        return name_;
    }
    [[nodiscard]] virtual size_t Count() const = 0;
};

template <typename C>
concept ClassLike = std::derived_from<C, Class>;

template <typename T>
requires std::is_arithmetic_v<T>
class PrimitiveClass : public Class {
public:
    explicit PrimitiveClass(std::string name) : Class(std::move(name)) {
    }
    [[nodiscard]] std::string Serialize() const override {
        std::string result = "_";
        result += type_name<T>();
        result.append("@").append(name_).append("_");
        return {result.begin(), std::remove_if(result.begin(), result.end(), isspace)};
    }
    [[nodiscard]] std::optional<size_t> Size() const override {
        return sizeof(T);
    }

    [[nodiscard]] size_t Count() const override {
        return 1;
    }
};

class StringClass : public Class {
public:
    explicit StringClass(std::string name) : Class(std::move(name)) {
    }
    [[nodiscard]] std::string Serialize() const override {
        return "_string@" + name_ + "_";
    }
    [[nodiscard]] std::optional<size_t> Size() const override {
        return std::nullopt;
    }

    [[nodiscard]] size_t Count() const override {
        return 1;
    }
};

class StructClass : public Class {
    std::vector<std::shared_ptr<Class>> fields_;

public:
    StructClass(std::string name) : Class(std::move(name)) {
    }
    template <ClassLike C>
    void AddField(C field) {
        fields_.push_back(std::make_shared<C>(field));
    }

    template <ClassLike C>
    void AddField(const std::shared_ptr<C>& field) {
        fields_.push_back(field);
    }

    [[nodiscard]] std::string Serialize() const override {
        auto result = "_struct@" + name_ + "_<";
        for (auto& field : fields_) {
            result.append(field->Serialize());
        }
        result.append(">");
        return result;
    }

    [[nodiscard]] std::optional<size_t> Size() const override {
        auto result = 0;
        for (auto& field : fields_) {
            auto size = field->Size();
            if (size.has_value()) {
                result += size.value();
            } else {
                return std::nullopt;
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<std::shared_ptr<Class>>& GetFields() {
        return fields_;
    }

    [[nodiscard]] size_t Count() const override {
        size_t count = 0;
        for (auto& field : fields_) {
            count += field->Count();
        }
        return count;
    }
};

template <ClassLike C, typename... Classes>
[[nodiscard]] std::shared_ptr<C> NewClass(std::string&& name, Classes&&... classes) {
    if constexpr (std::is_same_v<C, StructClass>) {
        auto new_class = std::make_shared<C>(std::move(name));
        (new_class->AddField(classes), ...);
        return new_class;
    } else {
        static_assert(sizeof...(classes) == 0);
        return std::make_shared<C>(std::move(name));
    }
}

///////////////////////////////////////////////////////////////////////
// Some inspiration for Type system realization from Yuko
///////////////////////////////////////////////////////////////////////

/*
template <class T>
typedef T* (*supplier)();

class MyFieldInfo {
protected:
    std::string fname;
    int size;
    int fieldOffset;

public:
    MyFieldInfo(std::string&& name, int size, int fieldOffset) {
        this->name = name;
        this->size = size;
        this->fieldOffset = fieldOffset;
    }

    void rawSetValue(T* obj, void* value) {
        memcpy(obj + fieldOffset, value, size);
    }

    void rawGetValue(T* obj, void* value) {
        memcpy(value, obj + fieldOffset, size);
    }
};

template <class T>
class MyTypeFieldInfoBase : public MyFieldInfo {
    MyTypeFieldInfoBase(std::string&& name, int size, int fieldOffset)
        : MyFieldInfo(name, size, fieldOffset) {
    }
}

template <class T, class F>
class MyTypeFieldInfo : public MyTypeFieldInfoBase<T> {
private:
    F T::*fieldRelPtr;

public:
    typedef F T::*fieldRelPtrType;

    MyTypeFieldInfo(std::string&& fname, fieldRelPtrType fieldRelPtr)
        : MyTypeFieldInfoBase(name, sizeof(F), (int)fieldRelPtr) {
        this->fieldRelPtr = fieldRelPtr;
    }

    void setValue(T* obj, F value) {
        obj.*fieldRelPtr = value;
    }

    F getValue(T* obj) {
        return obj.*fieldRelPtr;
    }
};

class MyTypeInfoBase {
protected:
    std::string tname;
    std::vector<MyFieldInfo> fields;

public:
    MyTypeInfoBase(std::string&& tname, MyTypeFieldInfoBase<T>... fields) {
        this->tname = tname;
        for (auto&& f : {fields...}) {
            this->fields.push_back(f);
        }
    }

    virtual void* createInstance() = 0;
};

template <class T>
class MyTypeInfo : public MyTypeInfoBase {
private:
    std::string tname;
    supplier<T> ctor;
    std::vector<MyTypeFieldInfoBase<T>> fields;

public:
    MyTypeInfo(std::string&& tname, supplier<T> ctor, MyTypeFieldInfoBase<T>... fields)
        : MyTypeInfoBase(name, fields) {
        this->tname = tname;
        this->ctor = ctor;
        for (auto&& f : {fields...}) {
            this->fields.push_back(f);
        }
    }

    void* createInstance() override {
        return this->ctor();
    }
};

#define F(fname, fref) MyTypeFieldInfo(fname, fref)
#define F(fref) MyTypeFieldInfo(#fref, fref)

typedef struct {
    float x, y, z;
} Vec3f;

typedef struct {
    Vec3f a, b;
} Rectf;

void test() {

    MyTypeInfo<Vec3f> vt("ttt", []() -> new Vec3f(), F(&Vec3f::x), F(&Vec3f::y), F(&Vec3f::z));

    iterator<T> it;

    while (it.next()) {
        T obj;
        it.fill(obj);
    }
}
*/

///////////////////////////////////////////////////////////////////////

}  // namespace ts