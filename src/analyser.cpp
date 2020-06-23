/*
Copyright libCellML Contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "libcellml/analyser.h"

#include <list>

#include "libcellml/analysermodel.h"
#include "libcellml/analyservariable.h"
#include "libcellml/component.h"
#include "libcellml/model.h"
#include "libcellml/units.h"
#include "libcellml/validator.h"
#include "libcellml/variable.h"

#include "analyserequationast.h"
#include "analysermodel_p.h"
#include "analyservariable_p.h"
#include "utilities.h"
#include "xmldoc.h"

#ifdef __linux__
#    undef TRUE
#    undef FALSE
#endif

namespace libcellml {

static const size_t MAX_SIZE_T = std::numeric_limits<size_t>::max();

struct AnalyserEquation;
using AnalyserEquationPtr = std::shared_ptr<AnalyserEquation>;
using AnalyserEquationWeakPtr = std::weak_ptr<AnalyserEquation>;

struct AnalyserInternalVariable
{
    enum struct Type
    {
        UNKNOWN,
        SHOULD_BE_STATE,
        VARIABLE_OF_INTEGRATION,
        STATE,
        CONSTANT,
        COMPUTED_TRUE_CONSTANT,
        COMPUTED_VARIABLE_BASED_CONSTANT,
        ALGEBRAIC,
        OVERCONSTRAINED
    };

    size_t mIndex = MAX_SIZE_T;
    Type mType = Type::UNKNOWN;

    VariablePtr mInitialisingVariable;
    VariablePtr mVariable;

    AnalyserEquationWeakPtr mEquation;

    explicit AnalyserInternalVariable(const VariablePtr &variable);

    void setVariable(const VariablePtr &variable,
                     bool checkInitialValue = true);

    void makeVoi();
    void makeState();
};

using AnalyserInternalVariablePtr = std::shared_ptr<AnalyserInternalVariable>;

AnalyserInternalVariable::AnalyserInternalVariable(const VariablePtr &variable)
{
    setVariable(variable);
}

void AnalyserInternalVariable::setVariable(const VariablePtr &variable,
                                           bool checkInitialValue)
{
    if (checkInitialValue && !variable->initialValue().empty()) {
        // The variable has an initial value, so it can either be a constant or
        // a state. By default, we consider it to be a constant and, if we find
        // an ODE for that variable, we will know that it was actually a state.

        mType = Type::CONSTANT;

        mInitialisingVariable = variable;
    }

    mVariable = variable;
}

void AnalyserInternalVariable::makeVoi()
{
    mType = Type::VARIABLE_OF_INTEGRATION;
}

void AnalyserInternalVariable::makeState()
{
    if (mType == Type::UNKNOWN) {
        mType = Type::SHOULD_BE_STATE;
    } else if (mType == Type::CONSTANT) {
        mType = Type::STATE;
    }
}

#ifdef SWIG
struct AnalyserEquation
#else
struct AnalyserEquation: public std::enable_shared_from_this<AnalyserEquation>
#endif
{
    enum struct Type
    {
        UNKNOWN,
        TRUE_CONSTANT,
        VARIABLE_BASED_CONSTANT,
        RATE,
        ALGEBRAIC
    };

    size_t mOrder = MAX_SIZE_T;
    Type mType = Type::UNKNOWN;

    std::list<AnalyserEquationPtr> mDependencies;

    AnalyserEquationAstPtr mAst;

    std::list<AnalyserInternalVariablePtr> mVariables;
    std::list<AnalyserInternalVariablePtr> mOdeVariables;

    AnalyserInternalVariablePtr mVariable = nullptr;
    ComponentPtr mComponent = nullptr;

    bool mComputedTrueConstant = true;
    bool mComputedVariableBasedConstant = true;

    bool mIsStateRateBased = false;

    explicit AnalyserEquation(const ComponentPtr &component);

    void addVariable(const AnalyserInternalVariablePtr &variable);
    void addOdeVariable(const AnalyserInternalVariablePtr &odeVariable);

    static bool containsNonUnknownVariables(const std::list<AnalyserInternalVariablePtr> &variables);
    static bool containsNonConstantVariables(const std::list<AnalyserInternalVariablePtr> &variables);

    static bool knownVariable(const AnalyserInternalVariablePtr &variable);
    static bool knownOdeVariable(const AnalyserInternalVariablePtr &odeVariable);

    bool check(size_t & equationOrder, size_t & stateIndex, size_t & variableIndex);
};

AnalyserEquation::AnalyserEquation(const ComponentPtr &component)
    : mAst(std::make_shared<AnalyserEquationAst>())
    , mComponent(component)
{
}

void AnalyserEquation::addVariable(const AnalyserInternalVariablePtr &variable)
{
    if (std::find(mVariables.begin(), mVariables.end(), variable) == mVariables.end()) {
        mVariables.push_back(variable);
    }
}

void AnalyserEquation::addOdeVariable(const AnalyserInternalVariablePtr &odeVariable)
{
    if (std::find(mOdeVariables.begin(), mOdeVariables.end(), odeVariable) == mOdeVariables.end()) {
        mOdeVariables.push_back(odeVariable);
    }
}

bool AnalyserEquation::containsNonUnknownVariables(const std::list<AnalyserInternalVariablePtr> &variables)
{
    return std::find_if(variables.begin(), variables.end(), [](const AnalyserInternalVariablePtr &variable) {
               return (variable->mType != AnalyserInternalVariable::Type::UNKNOWN);
           })
           != std::end(variables);
}

bool AnalyserEquation::containsNonConstantVariables(const std::list<AnalyserInternalVariablePtr> &variables)
{
    return std::find_if(variables.begin(), variables.end(), [](const AnalyserInternalVariablePtr &variable) {
               return (variable->mType != AnalyserInternalVariable::Type::UNKNOWN)
                      && (variable->mType != AnalyserInternalVariable::Type::CONSTANT)
                      && (variable->mType != AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT)
                      && (variable->mType != AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT);
           })
           != std::end(variables);
}

bool AnalyserEquation::knownVariable(const AnalyserInternalVariablePtr &variable)
{
    return (variable->mIndex != MAX_SIZE_T)
           || (variable->mType == AnalyserInternalVariable::Type::VARIABLE_OF_INTEGRATION)
           || (variable->mType == AnalyserInternalVariable::Type::STATE)
           || (variable->mType == AnalyserInternalVariable::Type::CONSTANT)
           || (variable->mType == AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT)
           || (variable->mType == AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT);
}

bool AnalyserEquation::knownOdeVariable(const AnalyserInternalVariablePtr &odeVariable)
{
    return (odeVariable->mIndex != MAX_SIZE_T)
           || (odeVariable->mType == AnalyserInternalVariable::Type::VARIABLE_OF_INTEGRATION);
}

bool AnalyserEquation::check(size_t &equationOrder, size_t &stateIndex,
                             size_t &variableIndex)
{
    // Nothing to check if the equation has already been given an order (i.e.
    // everything is fine) or if there is one known (ODE) variable left (i.e.
    // this equation is an overconstraint).

    if (mOrder != MAX_SIZE_T) {
        return false;
    }

    if (mVariables.size() + mOdeVariables.size() == 1) {
        AnalyserInternalVariablePtr variable = (mVariables.size() == 1) ? mVariables.front() : mOdeVariables.front();

        if ((variable->mIndex != MAX_SIZE_T)
            && (variable->mType != AnalyserInternalVariable::Type::UNKNOWN)
            && (variable->mType != AnalyserInternalVariable::Type::SHOULD_BE_STATE)) {
            variable->mType = AnalyserInternalVariable::Type::OVERCONSTRAINED;

            return false;
        }
    }

    // Determine, from the (new) known (ODE) variables, whether the equation is
    // truly a constant or a variable-based constant.

    mComputedTrueConstant = mComputedTrueConstant
                            && !containsNonUnknownVariables(mVariables)
                            && !containsNonUnknownVariables(mOdeVariables);
    mComputedVariableBasedConstant = mComputedVariableBasedConstant
                                     && !containsNonConstantVariables(mVariables)
                                     && !containsNonConstantVariables(mOdeVariables);

    // Determine whether the equation is state/rate based and add, as a
    // dependency, the equations used to compute the (new) known variables.
    // (Note that we don't account for the (new) known ODE variables since their
    // corresponding equation can obviously not be of algebraic type.)

    if (!mIsStateRateBased) {
        mIsStateRateBased = !mOdeVariables.empty();
    }

    for (const auto &variable : mVariables) {
        if (knownVariable(variable)) {
            AnalyserEquationPtr equation = variable->mEquation.lock();

            if (!mIsStateRateBased) {
                mIsStateRateBased = (equation == nullptr) ?
                                        (variable->mType == AnalyserInternalVariable::Type::STATE) :
                                        equation->mIsStateRateBased;
            }

            if (equation != nullptr) {
                mDependencies.push_back(equation);
            }
        }
    }

    // Stop tracking (new) known (ODE) variables.

    mVariables.remove_if(knownVariable);
    mOdeVariables.remove_if(knownOdeVariable);

    // If there is one (ODE) variable left then update its viariable (to be the
    // corresponding one in the component in which the equation is), its type
    // (if it is currently unknown), determine its index and determine the type
    // of our equation and set its order, if the (ODE) variable is a state,
    // computed constant or algebraic variable.

    bool relevantCheck = false;

    if (mVariables.size() + mOdeVariables.size() == 1) {
        AnalyserInternalVariablePtr variable = (mVariables.size() == 1) ? mVariables.front() : mOdeVariables.front();

        for (size_t i = 0; i < mComponent->variableCount(); ++i) {
            VariablePtr localVariable = mComponent->variable(i);

            if (isSameOrEquivalentVariable(variable->mVariable, localVariable)) {
                variable->setVariable(localVariable, false);

                break;
            }
        }

        if (variable->mType == AnalyserInternalVariable::Type::UNKNOWN) {
            variable->mType = mComputedTrueConstant ?
                                  AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT :
                                  mComputedVariableBasedConstant ?
                                  AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT :
                                  AnalyserInternalVariable::Type::ALGEBRAIC;
        }

        if ((variable->mType == AnalyserInternalVariable::Type::STATE)
            || (variable->mType == AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT)
            || (variable->mType == AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT)
            || (variable->mType == AnalyserInternalVariable::Type::ALGEBRAIC)) {
            variable->mIndex = (variable->mType == AnalyserInternalVariable::Type::STATE) ?
                                   ++stateIndex :
                                   ++variableIndex;

            variable->mEquation = shared_from_this();

            mOrder = ++equationOrder;
            mType = (variable->mType == AnalyserInternalVariable::Type::STATE) ?
                        Type::RATE :
                        (variable->mType == AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT) ?
                        Type::TRUE_CONSTANT :
                        (variable->mType == AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT) ?
                        Type::VARIABLE_BASED_CONSTANT :
                        Type::ALGEBRAIC;

            mVariable = variable;

            relevantCheck = true;
        }
    }

    return relevantCheck;
}

/**
 * @brief The Analyser::AnalyserImpl struct.
 *
 * The private implementation for the Analyser class.
 */
struct Analyser::AnalyserImpl
{
    Analyser *mAnalyser = nullptr;

    AnalyserModelPtr mModel = nullptr;

    std::list<AnalyserInternalVariablePtr> mInternalVariables;
    std::list<AnalyserEquationPtr> mEquations;

    explicit AnalyserImpl(Analyser *analyser);

    static bool compareVariablesByName(const AnalyserInternalVariablePtr &variable1,
                                       const AnalyserInternalVariablePtr &variable2);

    static bool isStateVariable(const AnalyserInternalVariablePtr &variable);
    static bool isConstantOrAlgebraicVariable(const AnalyserInternalVariablePtr &variable);

    static bool compareVariablesByTypeAndIndex(const AnalyserInternalVariablePtr &variable1,
                                               const AnalyserInternalVariablePtr &variable2);

    static bool compareEquationsByVariable(const AnalyserEquationPtr &equation1,
                                           const AnalyserEquationPtr &equation2);

    size_t mathmlChildCount(const XmlNodePtr &node) const;
    XmlNodePtr mathmlChildNode(const XmlNodePtr &node, size_t index) const;

    AnalyserInternalVariablePtr analyserVariable(const VariablePtr &variable);

    VariablePtr voiFirstOccurrence(const VariablePtr &variable,
                                   const ComponentPtr &component);

    void processNode(const XmlNodePtr &node, AnalyserEquationAstPtr &ast,
                     const AnalyserEquationAstPtr &astParent,
                     const ComponentPtr &component,
                     const AnalyserEquationPtr &equation);
    AnalyserEquationPtr processNode(const XmlNodePtr &node,
                                    const ComponentPtr &component);
    void processComponent(const ComponentPtr &component);

    void doEquivalentVariables(const VariablePtr &variable,
                               std::vector<VariablePtr> &equivalentVariables) const;
    std::vector<VariablePtr> equivalentVariables(const VariablePtr &variable) const;

    void processEquationAst(const AnalyserEquationAstPtr &ast);

    double scalingFactor(const VariablePtr &variable);

    void scaleAst(const AnalyserEquationAstPtr &ast,
                  const AnalyserEquationAstPtr &astParent,
                  double scalingFactor);
    void scaleEquationAst(const AnalyserEquationAstPtr &ast);

    void processModel(const ModelPtr &model);
};

Analyser::AnalyserImpl::AnalyserImpl(Analyser *analyser)
    : mAnalyser(analyser)
    , mModel(AnalyserModel::create())
{
}

bool Analyser::AnalyserImpl::compareVariablesByName(const AnalyserInternalVariablePtr &variable1,
                                                    const AnalyserInternalVariablePtr &variable2)
{
    ComponentPtr realComponent1 = owningComponent(variable1->mVariable);
    ComponentPtr realComponent2 = owningComponent(variable2->mVariable);

    if (realComponent1->name() == realComponent2->name()) {
        return variable1->mVariable->name() < variable2->mVariable->name();
    }

    return realComponent1->name() < realComponent2->name();
}

bool Analyser::AnalyserImpl::isStateVariable(const AnalyserInternalVariablePtr &variable)
{
    return variable->mType == AnalyserInternalVariable::Type::STATE;
}

bool Analyser::AnalyserImpl::isConstantOrAlgebraicVariable(const AnalyserInternalVariablePtr &variable)
{
    return (variable->mType == AnalyserInternalVariable::Type::CONSTANT)
           || (variable->mType == AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT)
           || (variable->mType == AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT)
           || (variable->mType == AnalyserInternalVariable::Type::ALGEBRAIC);
}

bool Analyser::AnalyserImpl::compareVariablesByTypeAndIndex(const AnalyserInternalVariablePtr &variable1,
                                                            const AnalyserInternalVariablePtr &variable2)
{
    if (isStateVariable(variable1) && isConstantOrAlgebraicVariable(variable2)) {
        return true;
    }

    if (isConstantOrAlgebraicVariable(variable1) && isStateVariable(variable2)) {
        return false;
    }

    return variable1->mIndex < variable2->mIndex;
}

bool Analyser::AnalyserImpl::compareEquationsByVariable(const AnalyserEquationPtr &equation1,
                                                        const AnalyserEquationPtr &equation2)
{
    return compareVariablesByTypeAndIndex(equation1->mVariable, equation2->mVariable);
}

size_t Analyser::AnalyserImpl::mathmlChildCount(const XmlNodePtr &node) const
{
    // Return the number of child elements, in the MathML namespace, for the
    // given node.

    XmlNodePtr childNode = node->firstChild();
    size_t res = childNode->isMathmlElement() ? 1 : 0;

    while (childNode != nullptr) {
        childNode = childNode->next();

        if (childNode && childNode->isMathmlElement()) {
            ++res;
        }
    }

    return res;
}

XmlNodePtr Analyser::AnalyserImpl::mathmlChildNode(const XmlNodePtr &node,
                                                   size_t index) const
{
    // Return the nth child element of the given node, skipping anything that is
    // not int he MathML namespace.

    XmlNodePtr res = node->firstChild();
    size_t childNodeIndex = res->isMathmlElement() ? 0 : MAX_SIZE_T;

    while ((res != nullptr) && (childNodeIndex != index)) {
        res = res->next();

        if (res && res->isMathmlElement()) {
            ++childNodeIndex;
        }
    }

    return res;
}

AnalyserInternalVariablePtr Analyser::AnalyserImpl::analyserVariable(const VariablePtr &variable)
{
    // Find and return, if there is one, the analyser variable associated with
    // the given variable.

    for (const auto &internalVariable : mInternalVariables) {
        if (isSameOrEquivalentVariable(variable, internalVariable->mVariable)) {
            return internalVariable;
        }
    }

    // No analyser variable exists for the given variable, so create one, track
    // it and return it.

    AnalyserInternalVariablePtr internalVariable = std::make_shared<AnalyserInternalVariable>(variable);

    mInternalVariables.push_back(internalVariable);

    return internalVariable;
}

VariablePtr Analyser::AnalyserImpl::voiFirstOccurrence(const VariablePtr &variable,
                                                       const ComponentPtr &component)
{
    // Recursively look for the first occurrence of the given variable in the
    // given component.

    for (size_t i = 0; i < component->variableCount(); ++i) {
        VariablePtr localVariable = component->variable(i);

        if (isSameOrEquivalentVariable(variable, localVariable)) {
            return localVariable;
        }
    }

    VariablePtr voi = nullptr;

    for (size_t i = 0; i < component->componentCount() && voi == nullptr; ++i) {
        voi = voiFirstOccurrence(variable, component->component(i));
    }

    return voi;
}

void Analyser::AnalyserImpl::processNode(const XmlNodePtr &node,
                                         AnalyserEquationAstPtr &ast,
                                         const AnalyserEquationAstPtr &astParent,
                                         const ComponentPtr &component,
                                         const AnalyserEquationPtr &equation)
{
    // Basic content elements.

    if (node->isMathmlElement("apply")) {
        // We may have 2, 3 or more child nodes, e.g.
        //
        //                 +--------+
        //                 |   +    |
        //        "+a" ==> |  / \   |
        //                 | a  nil |
        //                 +--------+
        //
        //                 +-------+
        //                 |   +   |
        //       "a+b" ==> |  / \  |
        //                 | a   b |
        //                 +-------+
        //
        //                 +-------------+
        //                 |   +         |
        //                 |  / \        |
        //                 | a   +       |
        //                 |    / \      |
        // "a+b+c+d+e" ==> |   b   +     |
        //                 |      / \    |
        //                 |     c   +   |
        //                 |        / \  |
        //                 |       d   e |
        //                 +-------------+

        size_t childCount = mathmlChildCount(node);

        processNode(mathmlChildNode(node, 0), ast, astParent, component, equation);
        processNode(mathmlChildNode(node, 1), ast->mLeft, ast, component, equation);

        if (childCount >= 3) {
            AnalyserEquationAstPtr astRight;
            AnalyserEquationAstPtr tempAst;

            processNode(mathmlChildNode(node, childCount - 1), astRight, nullptr, component, equation);

            for (size_t i = childCount - 2; i > 1; --i) {
                processNode(mathmlChildNode(node, 0), tempAst, nullptr, component, equation);
                processNode(mathmlChildNode(node, i), tempAst->mLeft, tempAst, component, equation);

                astRight->mParent = tempAst;

                tempAst->mRight = astRight;
                astRight = tempAst;
            }

            if (astRight != nullptr) {
                astRight->mParent = ast;
            }

            ast->mRight = astRight;
        }

        // Assignment, and relational and logical operators.

    } else if (node->isMathmlElement("eq")) {
        // This element is used both to describe "a = b" and "a == b". We can
        // distinguish between the two by checking its grand-parent. If it's a
        // "math" element then it means that it is used to describe "a = b"
        // otherwise it is used to describe "a == b". In the former case, there
        // is nothing more we need to do since `ast` is already of
        // AnalyserEquationAst::Type::ASSIGNMENT type.

        if (!node->parent()->parent()->isMathmlElement("math")) {
            ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::EQ, astParent);

            mModel->mPimpl->mNeedEq = true;
        }
    } else if (node->isMathmlElement("neq")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::NEQ, astParent);

        mModel->mPimpl->mNeedNeq = true;
    } else if (node->isMathmlElement("lt")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::LT, astParent);

        mModel->mPimpl->mNeedLt = true;
    } else if (node->isMathmlElement("leq")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::LEQ, astParent);

        mModel->mPimpl->mNeedLeq = true;
    } else if (node->isMathmlElement("gt")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::GT, astParent);

        mModel->mPimpl->mNeedGt = true;
    } else if (node->isMathmlElement("geq")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::GEQ, astParent);

        mModel->mPimpl->mNeedGeq = true;
    } else if (node->isMathmlElement("and")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::AND, astParent);

        mModel->mPimpl->mNeedAnd = true;
    } else if (node->isMathmlElement("or")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::OR, astParent);

        mModel->mPimpl->mNeedOr = true;
    } else if (node->isMathmlElement("xor")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::XOR, astParent);

        mModel->mPimpl->mNeedXor = true;
    } else if (node->isMathmlElement("not")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::NOT, astParent);

        mModel->mPimpl->mNeedNot = true;

        // Arithmetic operators.

    } else if (node->isMathmlElement("plus")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::PLUS, astParent);
    } else if (node->isMathmlElement("minus")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::MINUS, astParent);
    } else if (node->isMathmlElement("times")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::TIMES, astParent);
    } else if (node->isMathmlElement("divide")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::DIVIDE, astParent);
    } else if (node->isMathmlElement("power")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::POWER, astParent);
    } else if (node->isMathmlElement("root")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ROOT, astParent);
    } else if (node->isMathmlElement("abs")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ABS, astParent);
    } else if (node->isMathmlElement("exp")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::EXP, astParent);
    } else if (node->isMathmlElement("ln")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::LN, astParent);
    } else if (node->isMathmlElement("log")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::LOG, astParent);
    } else if (node->isMathmlElement("ceiling")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::CEILING, astParent);
    } else if (node->isMathmlElement("floor")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::FLOOR, astParent);
    } else if (node->isMathmlElement("min")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::MIN, astParent);

        mModel->mPimpl->mNeedMin = true;
    } else if (node->isMathmlElement("max")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::MAX, astParent);

        mModel->mPimpl->mNeedMax = true;
    } else if (node->isMathmlElement("rem")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::REM, astParent);

        // Calculus elements.

    } else if (node->isMathmlElement("diff")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::DIFF, astParent);

        // Trigonometric operators.

    } else if (node->isMathmlElement("sin")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::SIN, astParent);
    } else if (node->isMathmlElement("cos")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::COS, astParent);
    } else if (node->isMathmlElement("tan")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::TAN, astParent);
    } else if (node->isMathmlElement("sec")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::SEC, astParent);

        mModel->mPimpl->mNeedSec = true;
    } else if (node->isMathmlElement("csc")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::CSC, astParent);

        mModel->mPimpl->mNeedCsc = true;
    } else if (node->isMathmlElement("cot")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::COT, astParent);

        mModel->mPimpl->mNeedCot = true;
    } else if (node->isMathmlElement("sinh")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::SINH, astParent);
    } else if (node->isMathmlElement("cosh")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::COSH, astParent);
    } else if (node->isMathmlElement("tanh")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::TANH, astParent);
    } else if (node->isMathmlElement("sech")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::SECH, astParent);

        mModel->mPimpl->mNeedSech = true;
    } else if (node->isMathmlElement("csch")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::CSCH, astParent);

        mModel->mPimpl->mNeedCsch = true;
    } else if (node->isMathmlElement("coth")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::COTH, astParent);

        mModel->mPimpl->mNeedCoth = true;
    } else if (node->isMathmlElement("arcsin")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ASIN, astParent);
    } else if (node->isMathmlElement("arccos")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ACOS, astParent);
    } else if (node->isMathmlElement("arctan")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ATAN, astParent);
    } else if (node->isMathmlElement("arcsec")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ASEC, astParent);

        mModel->mPimpl->mNeedAsec = true;
    } else if (node->isMathmlElement("arccsc")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ACSC, astParent);

        mModel->mPimpl->mNeedAcsc = true;
    } else if (node->isMathmlElement("arccot")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ACOT, astParent);

        mModel->mPimpl->mNeedAcot = true;
    } else if (node->isMathmlElement("arcsinh")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ASINH, astParent);
    } else if (node->isMathmlElement("arccosh")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ACOSH, astParent);
    } else if (node->isMathmlElement("arctanh")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ATANH, astParent);
    } else if (node->isMathmlElement("arcsech")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ASECH, astParent);

        mModel->mPimpl->mNeedAsech = true;
    } else if (node->isMathmlElement("arccsch")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ACSCH, astParent);

        mModel->mPimpl->mNeedAcsch = true;
    } else if (node->isMathmlElement("arccoth")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::ACOTH, astParent);

        mModel->mPimpl->mNeedAcoth = true;

        // Piecewise statement.

    } else if (node->isMathmlElement("piecewise")) {
        size_t childCount = mathmlChildCount(node);

        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::PIECEWISE, astParent);

        processNode(mathmlChildNode(node, 0), ast->mLeft, ast, component, equation);

        if (childCount >= 2) {
            AnalyserEquationAstPtr astRight;
            AnalyserEquationAstPtr tempAst;

            processNode(mathmlChildNode(node, childCount - 1), astRight, nullptr, component, equation);

            for (size_t i = childCount - 2; i > 0; --i) {
                tempAst = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::PIECEWISE, astParent);

                processNode(mathmlChildNode(node, i), tempAst->mLeft, tempAst, component, equation);

                astRight->mParent = tempAst;

                tempAst->mRight = astRight;
                astRight = tempAst;
            }

            astRight->mParent = ast;

            ast->mRight = astRight;
        }
    } else if (node->isMathmlElement("piece")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::PIECE, astParent);

        processNode(mathmlChildNode(node, 0), ast->mLeft, ast, component, equation);
        processNode(mathmlChildNode(node, 1), ast->mRight, ast, component, equation);
    } else if (node->isMathmlElement("otherwise")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::OTHERWISE, astParent);

        processNode(mathmlChildNode(node, 0), ast->mLeft, ast, component, equation);

        // Token elements.

    } else if (node->isMathmlElement("ci")) {
        std::string variableName = node->firstChild()->convertToString();
        VariablePtr variable = component->variable(variableName);
        // Note: we always have a variable. Indeed, if we were not to have one,
        //       it would mean that `variableName` is the name of a variable
        //       that is referenced in an equation, but not defined anywhere,
        //       something that is not allowed in CellML and will therefore be
        //       reported when we validate the model.

        // Have our equation track the (ODE) variable (by ODE variable, we mean
        // a variable that is used in a "diff" element).

        if (node->parent()->firstChild()->isMathmlElement("diff")) {
            equation->addOdeVariable(analyserVariable(variable));
        } else if (!(node->parent()->isMathmlElement("bvar")
                     && node->parent()->parent()->firstChild()->isMathmlElement("diff"))) {
            equation->addVariable(analyserVariable(variable));
        }

        // Add the variable to our AST.

        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::CI, variable, astParent);
    } else if (node->isMathmlElement("cn")) {
        if (mathmlChildCount(node) == 1) {
            // We are dealing with an e-notation based CN value.

            ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::CN, node->firstChild()->convertToString() + "e" + node->firstChild()->next()->next()->convertToString(), astParent);
        } else {
            ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::CN, node->firstChild()->convertToString(), astParent);
        }

        // Qualifier elements.

    } else if (node->isMathmlElement("degree")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::DEGREE, astParent);

        processNode(mathmlChildNode(node, 0), ast->mLeft, ast, component, equation);
    } else if (node->isMathmlElement("logbase")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::LOGBASE, astParent);

        processNode(mathmlChildNode(node, 0), ast->mLeft, ast, component, equation);
    } else if (node->isMathmlElement("bvar")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::BVAR, astParent);

        processNode(mathmlChildNode(node, 0), ast->mLeft, ast, component, equation);

        XmlNodePtr rightNode = mathmlChildNode(node, 1);

        if (rightNode != nullptr) {
            processNode(rightNode, ast->mRight, ast, component, equation);
        }

        // Constants.

    } else if (node->isMathmlElement("true")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::TRUE, astParent);
    } else if (node->isMathmlElement("false")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::FALSE, astParent);
    } else if (node->isMathmlElement("exponentiale")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::E, astParent);
    } else if (node->isMathmlElement("pi")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::PI, astParent);
    } else if (node->isMathmlElement("infinity")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::INF, astParent);
    } else if (node->isMathmlElement("notanumber")) {
        ast = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::NAN, astParent);
    }
}

AnalyserEquationPtr Analyser::AnalyserImpl::processNode(const XmlNodePtr &node,
                                                        const ComponentPtr &component)
{
    // Create and keep track of the equation associated with the given node.

    AnalyserEquationPtr equation = std::make_shared<AnalyserEquation>(component);

    mEquations.push_back(equation);

    // Actually process the node and return its corresponding equation.

    processNode(node, equation->mAst, equation->mAst->mParent.lock(), component, equation);

    return equation;
}

void Analyser::AnalyserImpl::processComponent(const ComponentPtr &component)
{
    // Retrieve the math string associated with the given component and process
    // it, one equation at a time.

    XmlDocPtr xmlDoc = std::make_shared<XmlDoc>();
    std::string math = component->math();

    if (!math.empty()) {
        xmlDoc->parseMathML(math, false);

        XmlNodePtr mathNode = xmlDoc->rootNode();

        for (XmlNodePtr node = mathNode->firstChild(); node != nullptr; node = node->next()) {
            if (node->isMathmlElement()) {
                processNode(node, component);
            }
        }
    }

    // Go through the given component's variables and make sure that everything
    // makes sense.

    for (size_t i = 0; i < component->variableCount(); ++i) {
        // Retrieve the variable's corresponding analyser variable.

        VariablePtr variable = component->variable(i);
        AnalyserInternalVariablePtr analyserVariable = Analyser::AnalyserImpl::analyserVariable(variable);

        // Replace the variable held by `analyserVariable`, in case the
        // existing one has no initial value while `variable` does and after
        // insuring that the initial value is either a double or an existing
        // variable. Otherwise, generate an error if the variable held by
        // `analyserVariable` and `variable` are both initialised.

        if (!variable->initialValue().empty()
            && analyserVariable->mVariable->initialValue().empty()) {
            analyserVariable->setVariable(variable);
        } else if ((variable != analyserVariable->mVariable)
                   && !variable->initialValue().empty()
                   && !analyserVariable->mVariable->initialValue().empty()) {
            IssuePtr issue = Issue::create();
            ComponentPtr trackedVariableComponent = owningComponent(analyserVariable->mVariable);

            issue->setDescription("Variable '" + variable->name()
                                  + "' in component '" + component->name()
                                  + "' of model '" + owningModel(component)->name()
                                  + "' and variable '" + analyserVariable->mVariable->name()
                                  + "' in component '" + trackedVariableComponent->name()
                                  + "' of model '" + owningModel(trackedVariableComponent)->name()
                                  + "' are equivalent and cannot therefore both be initialised.");
            issue->setCause(Issue::Cause::ANALYSER);

            mAnalyser->addIssue(issue);
        }

        if (!analyserVariable->mVariable->initialValue().empty()
            && !isCellMLReal(analyserVariable->mVariable->initialValue())) {
            // The initial value is not a double, so it has to be an existing
            // variable of constant type.
            // Note: we always have an initialising variable. Indeed, if we were
            //       not to have one, it would mean that the variable is
            //       initialised using a reference to a variable that is not
            //       defined anywhere, something that is not allowed in CellML
            //       and will therefore be reported when we validate the model.

            ComponentPtr initialisingComponent = owningComponent(analyserVariable->mVariable);
            VariablePtr initialisingVariable = initialisingComponent->variable(analyserVariable->mVariable->initialValue());
            AnalyserInternalVariablePtr analyserInitialValueVariable = Analyser::AnalyserImpl::analyserVariable(initialisingVariable);

            if (analyserInitialValueVariable->mType != AnalyserInternalVariable::Type::CONSTANT) {
                IssuePtr issue = Issue::create();

                issue->setDescription("Variable '" + variable->name()
                                      + "' in component '" + component->name()
                                      + "' of model '" + owningModel(component)->name()
                                      + "' is initialised using variable '" + analyserVariable->mVariable->initialValue()
                                      + "', but it is not a constant.");
                issue->setCause(Issue::Cause::ANALYSER);

                mAnalyser->addIssue(issue);
            }
        }
    }

    // Do the same for the components encapsulated by the given component.

    for (size_t i = 0; i < component->componentCount(); ++i) {
        processComponent(component->component(i));
    }
}

void Analyser::AnalyserImpl::doEquivalentVariables(const VariablePtr &variable,
                                                   std::vector<VariablePtr> &equivalentVariables) const
{
    for (size_t i = 0; i < variable->equivalentVariableCount(); ++i) {
        VariablePtr equivalentVariable = variable->equivalentVariable(i);

        if (std::find(equivalentVariables.begin(), equivalentVariables.end(), equivalentVariable) == equivalentVariables.end()) {
            equivalentVariables.push_back(equivalentVariable);

            doEquivalentVariables(equivalentVariable, equivalentVariables);
        }
    }
}

std::vector<VariablePtr> Analyser::AnalyserImpl::equivalentVariables(const VariablePtr &variable) const
{
    std::vector<VariablePtr> res = {variable};

    doEquivalentVariables(variable, res);

    return res;
}

void Analyser::AnalyserImpl::processEquationAst(const AnalyserEquationAstPtr &ast)
{
    // Look for the definition of a variable of integration and make sure that
    // we don't have more than one of it and that it's not initialised.

    AnalyserEquationAstPtr astParent = ast->mParent.lock();
    AnalyserEquationAstPtr astGrandParent = (astParent != nullptr) ? astParent->mParent.lock() : nullptr;
    AnalyserEquationAstPtr astGreatGrandParent = (astGrandParent != nullptr) ? astGrandParent->mParent.lock() : nullptr;

    if ((ast->mType == AnalyserEquationAst::Type::CI)
        && (astParent != nullptr) && (astParent->mType == AnalyserEquationAst::Type::BVAR)
        && (astGrandParent != nullptr) && (astGrandParent->mType == AnalyserEquationAst::Type::DIFF)) {
        VariablePtr variable = ast->mVariable;

        analyserVariable(variable)->makeVoi();
        // Note: we must make the variable a variable of integration in all
        //       cases (i.e. even if there is, for example, already another
        //       variable of integration) otherwise unnecessary issue messages
        //       may be reported (since the type of the variable would be
        //       unknown).

        if (mModel->mPimpl->mVoi == nullptr) {
            // We have found our variable of integration, but this may not be
            // the one defined in our first component (i.e. the component under
            // which we are likely to expect to see the variable of integration
            // to be defined), so go through our components and look for the
            // first occurrence of our variable of integration.

            ModelPtr model = owningModel(variable);

            for (size_t i = 0; i < model->componentCount(); ++i) {
                VariablePtr voi = voiFirstOccurrence(variable, model->component(i));

                if (voi != nullptr) {
                    // We have found the first occurrence of our variable of
                    // integration, but now we must ensure that it (or one of
                    // its equivalent variables) is not initialised.

                    bool isVoiInitialized = false;

                    for (const auto &voiEquivalentVariable : equivalentVariables(voi)) {
                        if (!voiEquivalentVariable->initialValue().empty()) {
                            IssuePtr issue = Issue::create();

                            issue->setDescription("Variable '" + voiEquivalentVariable->name()
                                                  + "' in component '" + owningComponent(voiEquivalentVariable)->name()
                                                  + "' of model '" + owningModel(voiEquivalentVariable)->name()
                                                  + "' cannot be both a variable of integration and initialised.");
                            issue->setCause(Issue::Cause::ANALYSER);

                            mAnalyser->addIssue(issue);

                            isVoiInitialized = true;
                        }
                    }

                    if (!isVoiInitialized) {
                        mModel->mPimpl->mVoi = AnalyserVariable::create();

                        mModel->mPimpl->mVoi->mPimpl->populate(nullptr, voi,
                                                               AnalyserVariable::Type::VARIABLE_OF_INTEGRATION);
                    }

                    break;
                }
            }
        } else if (!isSameOrEquivalentVariable(variable, mModel->mPimpl->mVoi->variable())) {
            IssuePtr issue = Issue::create();

            issue->setDescription("Variable '" + mModel->mPimpl->mVoi->variable()->name()
                                  + "' in component '" + owningComponent(mModel->mPimpl->mVoi->variable())->name()
                                  + "' of model '" + owningModel(mModel->mPimpl->mVoi->variable())->name()
                                  + "' and variable '" + variable->name()
                                  + "' in component '" + owningComponent(variable)->name()
                                  + "' of model '" + owningModel(variable)->name()
                                  + "' cannot both be the variable of integration.");
            issue->setCause(Issue::Cause::ANALYSER);

            mAnalyser->addIssue(issue);
        }
    }

    // Make sure that we only use first-order ODEs.

    if ((ast->mType == AnalyserEquationAst::Type::CN)
        && (astParent != nullptr) && (astParent->mType == AnalyserEquationAst::Type::DEGREE)
        && (astGrandParent != nullptr) && (astGrandParent->mType == AnalyserEquationAst::Type::BVAR)
        && (astGreatGrandParent != nullptr) && (astGreatGrandParent->mType == AnalyserEquationAst::Type::DIFF)) {
        double value;
        if (!convertToDouble(ast->mValue, value) || !areEqual(value, 1.0)) {
            IssuePtr issue = Issue::create();
            VariablePtr variable = astGreatGrandParent->mRight->mVariable;

            issue->setDescription("The differential equation for variable '" + variable->name()
                                  + "' in component '" + owningComponent(variable)->name()
                                  + "' of model '" + owningModel(variable)->name()
                                  + "' must be of the first order.");
            issue->setCause(Issue::Cause::ANALYSER);

            mAnalyser->addIssue(issue);
        }
    }

    // Make a variable a state if it is used in an ODE.

    if ((ast->mType == AnalyserEquationAst::Type::CI)
        && (astParent != nullptr) && (astParent->mType == AnalyserEquationAst::Type::DIFF)) {
        analyserVariable(ast->mVariable)->makeState();
    }

    // Recursively check the given AST's children.

    if (ast->mLeft != nullptr) {
        processEquationAst(ast->mLeft);
    }

    if (ast->mRight != nullptr) {
        processEquationAst(ast->mRight);
    }
}

double Analyser::AnalyserImpl::scalingFactor(const VariablePtr &variable)
{
    // Return the scaling factor for the given variable.

    return Units::scalingFactor(variable->units(),
                                analyserVariable(variable)->mVariable->units());
}

void Analyser::AnalyserImpl::scaleAst(const AnalyserEquationAstPtr &ast,
                                      const AnalyserEquationAstPtr &astParent,
                                      double scalingFactor)
{
    // Scale the given AST using the given scaling factor

    AnalyserEquationAstPtr scaledAst = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::TIMES, astParent);

    scaledAst->mLeft = std::make_shared<AnalyserEquationAst>(AnalyserEquationAst::Type::CN, convertToString(scalingFactor), scaledAst);
    scaledAst->mRight = ast;

    ast->mParent = scaledAst;

    if (astParent->mLeft == ast) {
        astParent->mLeft = scaledAst;
    } else {
        astParent->mRight = scaledAst;
    }
}

void Analyser::AnalyserImpl::scaleEquationAst(const AnalyserEquationAstPtr &ast)
{
    // Recursively scale the given AST's children.

    if (ast->mLeft != nullptr) {
        scaleEquationAst(ast->mLeft);
    }

    if (ast->mRight != nullptr) {
        scaleEquationAst(ast->mRight);
    }

    // If the given AST node is a variabe (i.e. a CI node) then we may need to
    // do some scaling.

    if (ast->mType == AnalyserEquationAst::Type::CI) {
        // The kind of scaling we may end up doing depends on whether we are
        // dealing with a rate or some other variable, i.e. whether or not it
        // has a DIFF node as a parent.

        AnalyserEquationAstPtr astParent = ast->mParent.lock();
        if (astParent->mType == AnalyserEquationAst::Type::DIFF) {
            // We are dealing with a rate, so retrieve the scaling factor for
            // its corresponding variable of integration and apply it, if
            // needed.

            double scalingFactor = Analyser::AnalyserImpl::scalingFactor(astParent->mLeft->mLeft->mVariable);

            if (!areEqual(scalingFactor, 1.0)) {
                // We need to scale using the inverse of the scaling factor, but
                // how we do it depends on whether the rate is to be computed or
                // used.

                AnalyserEquationAstPtr astGrandParent = astParent->mParent.lock();

                if ((astGrandParent->mType == AnalyserEquationAst::Type::ASSIGNMENT)
                    && (astGrandParent->mLeft == astParent)) {
                    scaleAst(astGrandParent->mRight, astGrandParent, 1.0 / scalingFactor);
                } else {
                    scaleAst(astParent, astGrandParent, 1.0 / scalingFactor);
                }
            }
        }

        if (((astParent->mType != AnalyserEquationAst::Type::ASSIGNMENT)
             || (astParent->mLeft != ast))
            && (astParent->mType != AnalyserEquationAst::Type::BVAR)) {
            // We are dealing with a variable which is neither a computed
            // variable nor our variable of integration, so retrieve its scaling
            // factor and apply it, if needed, distinguishing between a rate
            // variable and an algebraic variable.

            double scalingFactor = Analyser::AnalyserImpl::scalingFactor(ast->mVariable);

            if (!areEqual(scalingFactor, 1.0)) {
                if (astParent->mType == AnalyserEquationAst::Type::DIFF) {
                    scaleAst(astParent, astParent->mParent.lock(), scalingFactor);
                } else {
                    scaleAst(ast, astParent, scalingFactor);
                }
            }
        }
    }
}

void Analyser::AnalyserImpl::processModel(const ModelPtr &model)
{
    // Reset a few things in case this analyser was to be used to process more
    // than one model.

    mAnalyser->removeAllIssues();

    mModel = AnalyserModel::create();

    mInternalVariables.clear();
    mEquations.clear();

    // Recursively process the model's components, so that we end up with an AST
    // for each of the model's equations.

    for (size_t i = 0; i < model->componentCount(); ++i) {
        processComponent(model->component(i));
    }

    // Some more processing is needed, but it can only be done if we didn't come
    // across any errors during the processing of our components.

    if (mAnalyser->errorCount() == 0) {
        // Process our different equations' AST to determine the type of our
        // variables.

        for (const auto &equation : mEquations) {
            processEquationAst(equation->mAst);
        }
    }

    // Some post-processing is now needed, but it can only be done if we didn't
    // come across any errors during the processing of our equations' AST.

    if (mAnalyser->errorCount() == 0) {
        // Sort our variables, determine the index of our constant variables and
        // then loop over our equations, checking which variables, if any, can
        // be determined using a given equation.

        mInternalVariables.sort(compareVariablesByName);

        size_t variableIndex = MAX_SIZE_T;

        for (const auto &internalVariable : mInternalVariables) {
            if (internalVariable->mType == AnalyserInternalVariable::Type::CONSTANT) {
                internalVariable->mIndex = ++variableIndex;
            }
        }

        size_t equationOrder = MAX_SIZE_T;
        size_t stateIndex = MAX_SIZE_T;

        for (;;) {
            bool relevantCheck = false;

            for (const auto &equation : mEquations) {
                relevantCheck = equation->check(equationOrder, stateIndex, variableIndex)
                                || relevantCheck;
            }

            if (!relevantCheck) {
                break;
            }
        }

        // Make sure that our variables are valid.

        for (const auto &internalVariable : mInternalVariables) {
            std::string issueType;

            switch (internalVariable->mType) {
            case AnalyserInternalVariable::Type::UNKNOWN:
                issueType = "is not computed";

                break;
            case AnalyserInternalVariable::Type::SHOULD_BE_STATE:
                issueType = "is used in an ODE, but it is not initialised";

                break;
            case AnalyserInternalVariable::Type::VARIABLE_OF_INTEGRATION:
            case AnalyserInternalVariable::Type::STATE:
            case AnalyserInternalVariable::Type::CONSTANT:
            case AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT:
            case AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT:
            case AnalyserInternalVariable::Type::ALGEBRAIC:
                break;
            case AnalyserInternalVariable::Type::OVERCONSTRAINED:
                issueType = "is computed more than once";

                break;
            }

            if (!issueType.empty()) {
                IssuePtr issue = Issue::create();
                VariablePtr realVariable = internalVariable->mVariable;

                issue->setDescription("Variable '" + realVariable->name()
                                      + "' in component '" + owningComponent(realVariable)->name()
                                      + "' of model '" + owningModel(realVariable)->name()
                                      + "' " + issueType + ".");
                issue->setCause(Issue::Cause::ANALYSER);

                mAnalyser->addIssue(issue);
            }
        }

        // Determine the type of our model.

        bool hasUnderconstrainedVariables = std::find_if(mInternalVariables.begin(), mInternalVariables.end(), [](const AnalyserInternalVariablePtr &variable) {
                                                return (variable->mType == AnalyserInternalVariable::Type::UNKNOWN)
                                                       || (variable->mType == AnalyserInternalVariable::Type::SHOULD_BE_STATE);
                                            })
                                            != std::end(mInternalVariables);
        bool hasOverconstrainedVariables = std::find_if(mInternalVariables.begin(), mInternalVariables.end(), [](const AnalyserInternalVariablePtr &variable) {
                                               return variable->mType == AnalyserInternalVariable::Type::OVERCONSTRAINED;
                                           })
                                           != std::end(mInternalVariables);

        if (hasUnderconstrainedVariables) {
            if (hasOverconstrainedVariables) {
                mModel->mPimpl->mType = AnalyserModel::Type::UNSUITABLY_CONSTRAINED;
            } else {
                mModel->mPimpl->mType = AnalyserModel::Type::UNDERCONSTRAINED;
            }
        } else if (hasOverconstrainedVariables) {
            mModel->mPimpl->mType = AnalyserModel::Type::OVERCONSTRAINED;
        } else if (mModel->mPimpl->mVoi != nullptr) {
            mModel->mPimpl->mType = AnalyserModel::Type::ODE;
        } else if (!mInternalVariables.empty()) {
            mModel->mPimpl->mType = AnalyserModel::Type::ALGEBRAIC;
        }
    } else {
        mModel->mPimpl->mType = AnalyserModel::Type::INVALID;
    }

    // Some final post-processing is now needed, if we have a valid model.

    if ((mModel->mPimpl->mType == AnalyserModel::Type::ODE)
        || (mModel->mPimpl->mType == AnalyserModel::Type::ALGEBRAIC)) {
        // Scale our equations' AST, i.e. take into account the fact that we may
        // have mapped variables that use compatible units rather than
        // equivalent ones.

        for (const auto &equation : mEquations) {
            scaleEquationAst(equation->mAst);
        }

        // Sort our variables and equations and make our internal variables
        // available through our API.

        mInternalVariables.sort(compareVariablesByTypeAndIndex);
        mEquations.sort(compareEquationsByVariable);

        for (const auto &internalVariable : mInternalVariables) {
            AnalyserVariable::Type type;

            if (internalVariable->mType == AnalyserInternalVariable::Type::STATE) {
                type = AnalyserVariable::Type::STATE;
            } else if (internalVariable->mType == AnalyserInternalVariable::Type::CONSTANT) {
                type = AnalyserVariable::Type::CONSTANT;
            } else if ((internalVariable->mType == AnalyserInternalVariable::Type::COMPUTED_TRUE_CONSTANT)
                       || (internalVariable->mType == AnalyserInternalVariable::Type::COMPUTED_VARIABLE_BASED_CONSTANT)) {
                type = AnalyserVariable::Type::COMPUTED_CONSTANT;
            } else if (internalVariable->mType == AnalyserInternalVariable::Type::ALGEBRAIC) {
                type = AnalyserVariable::Type::ALGEBRAIC;
            } else {
                // This is the variable of integration, so skip it.

                continue;
            }

            AnalyserVariablePtr stateOrVariable = AnalyserVariable::create();

            stateOrVariable->mPimpl->populate(internalVariable->mInitialisingVariable,
                                              internalVariable->mVariable,
                                              type);

            if (type == AnalyserVariable::Type::STATE) {
                mModel->mPimpl->mStates.push_back(stateOrVariable);
            } else {
                mModel->mPimpl->mVariables.push_back(stateOrVariable);
            }
        }
    }
}

Analyser::Analyser()
    : mPimpl(new AnalyserImpl {this})
{
}

Analyser::~Analyser()
{
    delete mPimpl;
}

AnalyserPtr Analyser::create() noexcept
{
    return AnalyserPtr {new Analyser {}};
}

void Analyser::processModel(const ModelPtr &model)
{
    // Make sure that the model is valid before processing it.

    ValidatorPtr validator = Validator::create();

    validator->validateModel(model);

    if (validator->issueCount() > 0) {
        // The model is not valid, so retrieve the validation issues and make
        // them our own.

        for (size_t i = 0; i < validator->issueCount(); ++i) {
            addIssue(validator->issue(i));
        }

        mPimpl->mModel->mPimpl->mType = AnalyserModel::Type::INVALID;

        return;
    }

    // Process the model.

    mPimpl->processModel(model);
}

AnalyserModelPtr Analyser::model() const
{
    return mPimpl->mModel;
}

} // namespace libcellml