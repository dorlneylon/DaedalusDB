#pragma once

#include "class.hpp"

#define DDB_PRIMITIVE_GENERATOR(MACRO) \
    MACRO(int)                         \
    MACRO(double)                      \
    MACRO(float)                       \
    MACRO(bool)                        \
    MACRO(unsigned int)                \
    MACRO(short int)                   \
    MACRO(short unsigned int)          \
    MACRO(long long int)               \
    MACRO(long long unsigned int)      \
    MACRO(long unsigned int)           \
    MACRO(long int)                    \
    MACRO(char)                        \
    MACRO(signed char)                 \
    MACRO(unsigned char)               \
    MACRO(wchar_t)

namespace ts {

class Object {
protected:
    std::shared_ptr<Class> class_;

public:
    virtual std::shared_ptr<Class> GetClass() {
        return class_;
    }
    virtual ~Object() = default;
    [[nodiscard]] virtual size_t Size() const {
        throw error::NotImplemented("Void Class");
    }
    virtual mem::Offset Write(std::shared_ptr<mem::File>& file, mem::Offset offset) const {
        throw error::NotImplemented("Void Class");
    }
    virtual void Read(std::shared_ptr<mem::File>& file, mem::Offset offset) {
        throw error::NotImplemented("Void Class");
    }
    [[nodiscard]] virtual std::string ToString() const {
        throw error::NotImplemented("Void Class");
    }
};

template <typename O>
concept ObjectLike = std::derived_from<O, Object>;

class ClassObject : public Object {
    std::shared_ptr<Class> class_holder_;
    std::string serialized_;
    using SizeType = u_int32_t;

    std::string ReadString(std::stringstream& stream, char end) {
        std::string result;
        char c;
        stream >> c;
        while (c != end) {
            result += c;
            stream >> c;
        }
        return result;
    }

    std::shared_ptr<Class> Deserialize(std::stringstream& stream) {
        char del;
        stream >> del;
        if (del == '>') {
            return nullptr;
        }
        if (del != '_') {
            throw error::TypeError("Can't read correct type by this address");
        }

        auto type = ReadString(stream, '@');
        if (type == "struct") {
            auto name = ReadString(stream, '_');
            stream >> del;
            if (del != '<') {
                throw error::TypeError("Can't read correct type by this address");
            }

            auto result = std::make_shared<StructClass>(name);

            auto field = Deserialize(stream);
            while (field != nullptr) {
                result->AddField(field);
                field = Deserialize(stream);
            }
            return result;
        } else if (type == "string") {
            return std::make_shared<StringClass>(ReadString(stream, '_'));
        } else {

            auto remove_spaces = [](const char* str) -> std::string {
                std::string s = str;
                return {s.begin(), remove_if(s.begin(), s.end(), isspace)};
            };

#define DDB_DESERIALIZE_PRIMITIVE(P)                                         \
    if (type == remove_spaces(#P)) {                                         \
        return std::make_shared<PrimitiveClass<P>>(ReadString(stream, '_')); \
    }
            DDB_PRIMITIVE_GENERATOR(DDB_DESERIALIZE_PRIMITIVE)
#undef DESERIALIZE_PRIMITIVE

            throw error::NotImplemented("Unsupported for deserialization type");
        }
    }

public:
    ~ClassObject() = default;
    ClassObject(){};
    virtual std::shared_ptr<Class> GetClass() {
        return class_holder_;
    }
    ClassObject(const std::shared_ptr<Class>& holder) : class_holder_(holder) {
        serialized_ = class_holder_->Serialize();
    }
    ClassObject(std::string string) : serialized_(string) {
        std::stringstream stream(serialized_);
        class_holder_ = Deserialize(stream);
    }
    [[nodiscard]] size_t Size() const override {
        return serialized_.size() + sizeof(SizeType);
    }
    mem::Offset Write(std::shared_ptr<mem::File>& file, mem::Offset offset) const override {
        auto new_offset = file->Write<SizeType>(serialized_.size(), offset) + sizeof(SizeType);
        return file->Write(serialized_, new_offset);
    }
    void Read(std::shared_ptr<mem::File>& file, mem::Offset offset) override {
        SizeType size = file->Read<SizeType>(offset);
        serialized_ = file->ReadString(offset + sizeof(SizeType), size);
        std::stringstream stream{serialized_};
        class_holder_ = Deserialize(stream);
    }
    [[nodiscard]] std::string ToString() const override {
        return serialized_;
    }

    template <ClassLike C>
    [[nodiscard]] bool Contains(std::shared_ptr<C> other_class) {
        return serialized_.contains(ClassObject(other_class).serialized_);
    }
};

template <typename T>
requires std::is_arithmetic_v<T>
class Primitive : public Object {
    T value_;

public:
    ~Primitive() = default;
    Primitive(const std::shared_ptr<PrimitiveClass<T>>& argclass, T value) : value_(value) {
        this->class_ = argclass;
    }
    explicit Primitive(const std::shared_ptr<PrimitiveClass<T>>& argclass) : value_(T{}) {
        this->class_ = argclass;
    }
    [[nodiscard]] size_t Size() const override {
        return sizeof(T);
    }
    [[nodiscard]] T Value() const {
        return value_;
    }
    [[nodiscard]] T& Value() {
        return value_;
    }
    mem::Offset Write(std::shared_ptr<mem::File>& file, mem::Offset offset) const override {
        return file->Write<T>(value_, offset);
    }
    void Read(std::shared_ptr<mem::File>& file, mem::Offset offset) override {
        value_ = file->Read<T>(offset);
    }
    [[nodiscard]] std::string ToString() const override {
        if constexpr (std::is_same_v<bool, T>) {
            return class_->Name() + ": " + (value_ ? "true" : "false");
        }
        return class_->Name() + ": " + std::to_string(value_);
    }
};

class String : public Object {
    std::string str_;
    using SizeType = u_int32_t;

public:
    ~String() = default;

    String(const std::shared_ptr<StringClass>& argclass, std::string str) : str_(std::move(str)) {
        this->class_ = argclass;
    }
    explicit String(const std::shared_ptr<StringClass>& argclass) : str_("") {
        this->class_ = argclass;
    }
    [[nodiscard]] size_t Size() const override {
        return str_.size() + sizeof(SizeType);
    }
    [[nodiscard]] std::string Value() const {
        return str_;
    }
    [[nodiscard]] std::string& Value() {
        return str_;
    }
    mem::Offset Write(std::shared_ptr<mem::File>& file, mem::Offset offset) const override {
        auto new_offset = file->Write<SizeType>(str_.size(), offset) + sizeof(SizeType);
        return file->Write(str_, new_offset);
    }
    void Read(std::shared_ptr<mem::File>& file, mem::Offset offset) override {
        SizeType size = file->Read<SizeType>(offset);
        str_ = file->ReadString(offset + sizeof(SizeType), size);
    }
    [[nodiscard]] std::string ToString() const override {
        return class_->Name() + ": \"" + str_ + "\"";
    }
};

class Struct : public Object {
    std::vector<std::shared_ptr<Object>> fields_;

    template <ObjectLike O>
    void AddFieldValue(const std::shared_ptr<O>& value) {
        fields_.push_back(value);
    }

public:
    template <ObjectLike O, ClassLike C, typename... Args>
    friend std::shared_ptr<O> UnsafeNew(std::shared_ptr<C> object_class, Args&&... args);
    template <ObjectLike O, ClassLike C>
    friend std::shared_ptr<O> DefaultNew(std::shared_ptr<C> object_class);

    ~Struct() = default;
    Struct(const std::shared_ptr<StructClass>& argclass) {
        this->class_ = argclass;
    }
    [[nodiscard]] size_t Size() const override {
        size_t size = 0;
        for (auto& field : fields_) {
            size += field->Size();
        }
        return size;
    }

    [[nodiscard]] std::vector<std::shared_ptr<Object>> GetFields() const {
        return fields_;
    }

    template <ObjectLike O>
    [[nodiscard]] std::shared_ptr<O> GetField(std::string name) const {
        for (auto& field : fields_) {
            if (field->GetClass()->Name() == name) {
                return util::As<O>(field);
            }
        }
        throw error::RuntimeError("No such field");
    }

    mem::Offset Write(std::shared_ptr<mem::File>& file, mem::Offset offset) const override {
        mem::Offset new_offset = offset;
        for (auto& field : fields_) {
            field->Write(file, new_offset);
            new_offset += field->Size();
        }
        return new_offset;
    }
    void Read(std::shared_ptr<mem::File>& file, mem::Offset offset) override {
        mem::Offset new_offset = offset;
        for (auto& field : fields_) {
            field->Read(file, new_offset);
            new_offset += field->Size();
        }
    }
    [[nodiscard]] std::string ToString() const override {
        std::string result = class_->Name() + ": { ";
        for (auto& field : fields_) {
            if (field != *fields_.begin()) {
                result.append(", ");
            }
            result.append(field->ToString());
        }
        result.append(" }");
        return result;
    }
};

template <ObjectLike O, ClassLike C, typename... Args>
[[nodiscard]] std::shared_ptr<O> UnsafeNew(std::shared_ptr<C> object_class, Args&&... args) {

    if constexpr (std::is_same_v<O, Struct>) {
        auto new_object = std::make_shared<Struct>(object_class);
        auto it = util::As<StructClass>(object_class)->GetFields().begin();
        (
            [&it, &args, &new_object] {
                auto class_ = *it++;
                if (util::Is<StructClass>(class_)) {
                    new_object->AddFieldValue(
                        UnsafeNew<Struct>(util::As<StructClass>(class_), args));
                } else if (util::Is<StringClass>(class_)) {
                    if constexpr (std::is_convertible_v<decltype(args), std::string_view>) {
                        new_object->AddFieldValue(
                            UnsafeNew<String>(util::As<StringClass>(class_), args));
                    }
                } else {
                    if constexpr (!std::is_convertible_v<std::remove_reference_t<decltype(args)>,
                                                         std::string_view>) {

                        // TODO: May cause bugs e.g. passed 0 as double, decltype(0) = int;
                        new_object->AddFieldValue(
                            UnsafeNew<Primitive<std::remove_reference_t<decltype(args)>>>(
                                util::As<PrimitiveClass<std::remove_reference_t<decltype(args)>>>(
                                    class_),
                                args));
                    }
                }
            }(),
            ...);
        return new_object;
    } else if constexpr (std::is_same_v<O, String>) {
        return std::make_shared<String>(object_class, std::move(std::get<0>(std::tuple(args...))));
    } else {
        return std::make_shared<O>(object_class, std::get<0>(std::tuple(args...)));
    }
    throw error::TypeError("Can't create object");
}

template <ObjectLike O, ClassLike C, typename... Args>
[[nodiscard]] std::shared_ptr<O> New(std::shared_ptr<C> object_class, Args&&... args) {

    if (object_class->Count() != sizeof...(Args)) {
        throw error::BadArgument("Wrong number of arguments");
    }

    return UnsafeNew<O>(object_class, args...);
}

template <ObjectLike O, ClassLike C>
[[nodiscard]] std::shared_ptr<O> DefaultNew(std::shared_ptr<C> object_class) {

    if constexpr (std::is_same_v<O, Struct>) {
        auto new_object = std::make_shared<Struct>(object_class);
        auto fields = util::As<StructClass>(object_class)->GetFields();
        for (auto it = fields.begin(); it != fields.end(); ++it) {
            if (util::Is<StructClass>(*it)) {
                new_object->AddFieldValue(DefaultNew<Struct>(util::As<StructClass>(*it)));
            } else if (util::Is<StringClass>(*it)) {
                new_object->AddFieldValue(DefaultNew<String>(util::As<StringClass>(*it)));
            } else {
#define DDB_ADD_PRIMITIVE(P)                                                                   \
    if (util::Is<PrimitiveClass<P>>(*it)) {                                                    \
        new_object->AddFieldValue(DefaultNew<Primitive<P>>(util::As<PrimitiveClass<P>>(*it))); \
        continue;                                                                              \
    }
                DDB_PRIMITIVE_GENERATOR(DDB_ADD_PRIMITIVE)
#undef ADD_PRIMITIVE

                throw error::TypeError("Class can't be defaulted");
            }
        }
        return new_object;
    } else if constexpr (std::is_same_v<O, String>) {
        return std::make_shared<String>(object_class);
    } else {
        return std::make_shared<O>(object_class);
    }
    throw error::TypeError("Can't create object");
}

template <ObjectLike O, ClassLike C>
[[nodiscard]] std::shared_ptr<O> ReadNew(std::shared_ptr<C> object_class,
                                         std::shared_ptr<mem::File>& file, mem::Offset offset) {

    auto new_object = DefaultNew<O, C>(object_class);
    new_object->Read(file, offset);
    return new_object;
}

}  // namespace ts