#include "../include/statement.h"

#include <iostream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
const string ERROR_OPERATION = "Error: the operation cannot be performed: "s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[var_] = rv_->Execute(closure, context); // ?
    return closure.at(var_);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv):
    var_(std::move(var)), rv_(move(rv)) {
}

VariableValue::VariableValue(const std::string& var_name): dotted_ids_{var_name} {
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids): dotted_ids_(move(dotted_ids)) {
}

ObjectHolder VariableValue::Execute(Closure& closure, Context& /*context*/) {
    ObjectHolder result;
    Closure* p_closure = &closure;
    for (const auto &id : dotted_ids_) {
        if (!p_closure->count(id)) { throw runtime_error("Uncknown : " + GetStrDottedIds()); }
        result = p_closure->at(id);
        if (result.TryAs<runtime::ClassInstance>()) {
            p_closure = &result.TryAs<runtime::ClassInstance>()->Fields();
        }
    }
    return result;
}

std::string VariableValue::GetStrDottedIds() {
    static const char* const delim = ", ";
    std::ostringstream imploded;
    std::copy(dotted_ids_.begin(), dotted_ids_.end(), std::ostream_iterator<std::string>(imploded, delim));
    return imploded.str();
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    return make_unique<Print>(make_unique<VariableValue>(name));
}

Print::Print(unique_ptr<Statement> argument) {
    args_.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args): args_(std::move(args)) {
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    auto& output = context.GetOutputStream();
    for (auto it = args_.cbegin(); it != args_.cend(); ++it) {
        auto holder = (*it)->Execute(closure, context);
        if (it != args_.cbegin()) { output << ' '; }
        if (holder) {
            holder->Print(output, context);
        } else {
            output << "None";
        }
    }
    output << '\n';
    return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args):
                       object_{std::move(object)}, method_{std::move(method)},
                       args_{std::move(args)} {

}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    auto instance  = object_->Execute(closure, context).TryAs<runtime::ClassInstance>();
    if (instance->HasMethod(method_, args_.size())) {
        std::vector<runtime::ObjectHolder> args(args_.size());
        std::transform(args_.cbegin(), args_.cend(),
                       args.begin(),
                       [&](const auto& arg){ return arg->Execute(closure, context); });
        return instance->Call(method_, args, context);
    }
    return {};
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    auto holder = argument_->Execute(closure, context);
    if (holder) {
        std::stringstream ss;
        holder->Print(ss, context);
        return ObjectHolder::Own(runtime::String(ss.str()));
    } else {
        return ObjectHolder::Own(runtime::String("None"));
    }
}

namespace {
    #define NUM_BINARY_OPERATION(lhs, rhs, type, operation) {                                                 \
        auto _lhs = (lhs).TryAs<type>(), _rhs = (rhs).TryAs<type>();                                      \
        if (_lhs && _rhs && std::isfinite(_lhs->GetValue() operation _rhs->GetValue())) {                 \
            return ObjectHolder::Own(type(_lhs->GetValue() operation _rhs->GetValue()));                  \
        }                                                                                                 \
    }
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context), rhs = rhs_->Execute(closure, context);
    NUM_BINARY_OPERATION(lhs, rhs, runtime::Number, +)
    if(lhs.TryAs<runtime::String>() && rhs.TryAs<runtime::String>()){
        return ObjectHolder::Own(runtime::String(lhs.TryAs<runtime::String>()->GetValue() + rhs.TryAs<runtime::String>()->GetValue()));
    }
    if (const auto left_instance = lhs.TryAs<runtime::ClassInstance>()) {
        return left_instance->Call(ADD_METHOD, {rhs}, context);
    }
    throw std::runtime_error(ERROR_OPERATION + "Add"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context), rhs = rhs_->Execute(closure, context);
    NUM_BINARY_OPERATION(lhs, rhs, runtime::Number, -)
    throw std::runtime_error(ERROR_OPERATION + "Substruct"s );
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = lhs_->Execute(closure, context), rhs = rhs_->Execute(closure, context);
    NUM_BINARY_OPERATION(lhs, rhs, runtime::Number, *)
    throw std::runtime_error(ERROR_OPERATION + "Multiply"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context), rhs = rhs_->Execute(closure, context);
    NUM_BINARY_OPERATION(lhs, rhs, runtime::Number, /)
    throw std::runtime_error(ERROR_OPERATION + "Division"s);
}

#undef NUM_BINARY_OPERATION

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for (const auto &stmt: args_) {
        stmt->Execute(closure, context);
    }
    return ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    throw statement_->Execute(closure, context);
}

ClassDefinition::ClassDefinition(ObjectHolder cls): cls_(std::move(cls)) {
}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
    closure[cls_.TryAs<runtime::Class>()->GetName()] = cls_;
    return ObjectHolder::None();
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv):
                                 object_(std::move(object)), field_name_(std::move(field_name)),rv_(std::move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    auto obj = object_.Execute(closure, context).TryAs<runtime::ClassInstance>();
    if (obj) { return obj->Fields()[field_name_] = rv_->Execute(closure, context); }
    throw runtime_error("Error: is not class"s);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body):
               condition_(std::move(condition)), if_body_(std::move(if_body)),
               else_body_(std::move(else_body)) {
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    if (runtime::IsTrue(condition_->Execute(closure, context))) {
        return if_body_->Execute(closure, context);
    } else if (else_body_) {
        return else_body_->Execute(closure, context);
    }
    return runtime::ObjectHolder::None();
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context), rhs = rhs_->Execute(closure, context);
    if (lhs && rhs) { return ObjectHolder::Own(runtime::Bool{IsTrue(lhs) || IsTrue(rhs)}); }
    throw runtime_error("Invalid arguments");
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context), rhs = rhs_->Execute(closure, context);
    if (lhs && rhs) { return ObjectHolder::Own(runtime::Bool{IsTrue(lhs) && IsTrue(rhs)}); }
    throw runtime_error("Invalid arguments");
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    auto arg = argument_->Execute(closure, context);
    if (arg) { return ObjectHolder::Own(runtime::Bool{!IsTrue(arg)}); }
    throw runtime_error("Invalid arguments");
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(std::move(cmp)) {

}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    auto lhs = lhs_->Execute(closure, context), rhs = rhs_->Execute(closure, context);
    return ObjectHolder::Own(runtime::Bool{cmp_(lhs, rhs, context)});
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args):
    class_instance_(class_), args_(std::move(args)){
}

NewInstance::NewInstance(const runtime::Class& class_): class_instance_(class_) {
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if (class_instance_.HasMethod(INIT_METHOD, args_.size())) {
        std::vector<runtime::ObjectHolder> actual_args(args_.size());
        std::transform(args_.cbegin(), args_.cend(),
                       actual_args.begin(),
                       [&](const auto& arg){ return arg->Execute(closure, context); });
        class_instance_.Call(INIT_METHOD, actual_args, context);
    }
    return runtime::ObjectHolder::Share(class_instance_);
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body): body_(std::move(body)) {
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        body_->Execute(closure, context);
        return runtime::ObjectHolder::None();
    }  catch (runtime::ObjectHolder& result) {
        return result;
    }  catch (...) { throw; }
}

}  // namespace ast