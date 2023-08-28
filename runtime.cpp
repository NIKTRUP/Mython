#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

namespace {
    const string STRING_METHOD = "__str__"s;
    const string LESS_METHOD = "__lt__"s;
    const string EQUAL_METHOD = "__eq__"s;
} // namespace

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем не владеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return {};
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

bool IsTrue(const ObjectHolder& object) {
    if (auto obj = object.TryAs<Bool>()) {
        return obj->GetValue();
    }
    if (auto obj = object.TryAs<Number>()) {
        return obj->GetValue() != 0;
    }
    if (auto obj = object.TryAs<String>()) {
        return !(obj->GetValue().empty());
    }
    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {
    if (HasMethod(STRING_METHOD, 0U)) {
        Call(STRING_METHOD, {}, context)->Print(os, context);
    } else {
        os << this;
    }
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    auto *p_method = cls_.GetMethod(method);
    return p_method && p_method->formal_params.size() == argument_count;
}

Closure& ClassInstance::Fields() {
    return closure_;
}

const Closure& ClassInstance::Fields() const {
    return closure_;
}

ClassInstance::ClassInstance(const Class& cls): cls_(cls) {}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if (!HasMethod(method, actual_args.size())) {
        throw std::runtime_error("No method "s+method+"("+std::to_string(actual_args.size())+") in class "s+cls_.GetName());
    }
    auto method_ = cls_.GetMethod(method);
    Closure args;
    args["self"s] = ObjectHolder::Share(*this);

    size_t index = 0;
    for (auto &param : method_->formal_params) {
        args[param] = actual_args.at(index++);
    }

    return method_->body->Execute(args, context);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent):
        name_(std::move(name)),
        methods_(std::move(methods)),
        parent_(parent) {
    for (size_t i = 0; i < methods_.size(); ++i) {
        methods_by_name_[methods_[i].name] = i;
    }
}

const Method* Class::GetMethod(const std::string& name) const {
    return methods_by_name_.count(name) ? &methods_.at(methods_by_name_.at(name)) :
           (parent_  ? parent_->GetMethod(name) : nullptr);
}

void Class::Print(ostream& os, [[maybe_unused]] Context& context) {
    os << "Class "sv << name_;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    auto boolean = detail::BaseCompare(lhs, rhs, std::equal_to());
    if(boolean.has_value()){ return boolean.value(); }
    if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod(EQUAL_METHOD, 1U)) {
        return lhs.TryAs<ClassInstance>()->Call(EQUAL_METHOD, {rhs}, context).TryAs<Bool>()->GetValue();
    }
    if (!lhs && !rhs) {
        return true;
    }
    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    auto boolean = detail::BaseCompare(lhs, rhs, std::less());
    if(boolean.has_value()){ return boolean.value(); }
    if (lhs.TryAs<ClassInstance>() && lhs.TryAs<ClassInstance>()->HasMethod(LESS_METHOD, 1U)) {
        return lhs.TryAs<ClassInstance>()->Call(LESS_METHOD, {rhs}, context).TryAs<Bool>()->GetValue();
    }
    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !(Less(lhs, rhs, context) || Equal(lhs, rhs, context));
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Greater(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime