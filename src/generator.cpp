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

#include "libcellml/generator.h"

namespace libcellml{

using namespace libcellml::operators;

std::string CXX::returnType(types t)
{
    static const std::unordered_map<types, std::string, EnumClassHash> returnTypes = {
        {types::void_t,"void "},
        {types::double_t, "double "},
        {types::double_ct, "const double "},
        {types::double_pt, "double *"},
        {types::double_rt, "double &"}
    };

    return returnTypes.at(t);
}

std::string CXX::argType(types t)
{
    static const std::unordered_map<types, std::string, EnumClassHash> argTypes = {
        {types::void_t,"void "},
        {types::double_t, "double "},
        {types::double_ct, "const double "},
        {types::double_pt, "double *"},
        {types::double_rt, "double &"},
    };

    return argTypes.at(t);
}

struct Generator::GeneratorImpl
{
    Generator* mGenerator;

    void findVOI(std::string math);
    void findVOIHelper(XmlNodePtr node);
    void findInitialValues(ComponentPtr c);
    std::shared_ptr<libcellml::operators::Representable> parseMathML(std::string math);
    std::shared_ptr<libcellml::operators::Representable> parseNode(XmlNodePtr node);
    template <typename L = CXX>
        std::string doGenerateCode(ModelPtr m);
    template <typename L = CXX>
        std::string generateInitConsts();
    template <typename L = CXX>
        std::string generateComputeRates(
                std::shared_ptr<libcellml::operators::Representable> r);
    template <typename L = CXX>
        std::string generateComputeVariables();
    template <typename L = CXX>
        std::string generateStateAliases();
    template <typename L = CXX>
        std::string generateVoiAlias();

    std::string mVoi;
    std::vector<std::string> mStates;
    std::unordered_map<std::string,double> mInitialValues;
    std::string mCode = "";
};

Generator::Generator()
    : mPimpl(new GeneratorImpl())
{
    mPimpl->mGenerator = this;
}

Generator::~Generator()
{
    delete mPimpl;
}

template<typename L>
std::string Generator::GeneratorImpl::generateStateAliases()
{
    std::string s;
    std::ostringstream oss(s);
    for (size_t i = 0; i < mStates.size(); i++)
    {
        oss << "    "
            << L::argType(L::types::double_rt) << mStates[i] << " = " << L::dereferenceOp() << "(states + " << i
            << ")" << L::instructionDelimiter() << std::endl;
    }
    oss << std::endl;
    return oss.str();
}

template<typename L>
std::string Generator::GeneratorImpl::generateVoiAlias()
{
    std::string s;
    std::ostringstream oss(s);
    oss << "    "
        << L::argType(L::types::double_ct) << mVoi << " = voi" << L::instructionDelimiter() << std::endl;
    oss << std::endl;
    return oss.str();
}

template<typename L>
std::string Generator::GeneratorImpl::generateInitConsts()
{
    std::string s;
    std::ostringstream oss(s);
    oss << L::returnType(L::types::void_t) << "initConsts"
        << L::argListOp()
        << L::argType(L::types::double_pt)
        << "constants, "
        << L::argType(L::types::double_pt)
        << "rates, "
        << L::argType(L::types::double_pt)
        << "states"
        << L::argListCl() << std::endl
        << L::funBodyOp() << std::endl;

    oss << generateStateAliases() << std::endl;
    for (auto s : mInitialValues)
    {
        oss << "    " << s.first << " = "
            << std::setprecision(16) << s.second << L::instructionDelimiter() << std::endl;
    }
    oss << std::endl << L::funBodyCl();
    return oss.str();
}

template<typename L>
std::string Generator::GeneratorImpl::generateComputeRates(std::shared_ptr<Representable> r)
{
    std::string s;
    std::ostringstream oss(s);
    oss << L::returnType(L::types::void_t) << "computeRates"
        << L::argListOp()
        << L::argType(L::types::double_t)
        << "voi, "
        << L::argType(L::types::double_pt)
        << "constants, "
        << L::argType(L::types::double_pt)
        << "rates, "
        << L::argType(L::types::double_pt)
        << "states, "
        << L::argType(L::types::double_pt)
        << "algebraic"
        << L::argListCl() << std::endl
        << L::funBodyOp() << std::endl;

    oss << generateVoiAlias() << std::endl;
    oss << generateStateAliases() << std::endl;

    oss << "    rates[0] = "
        << r->repr() << ";" << std::endl;

    oss << std::endl
        << L::funBodyCl();
    return oss.str();
}

template<typename L>
std::string Generator::GeneratorImpl::generateComputeVariables()
{
    std::string s;
    std::ostringstream oss(s);
    oss << L::returnType(L::types::void_t) << "computeVariables"
        << L::argListOp()
        << L::argType(L::types::double_t)
        << "voi, "
        << L::argType(L::types::double_pt)
        << "constants, "
        << L::argType(L::types::double_pt)
        << "rates, "
        << L::argType(L::types::double_pt)
        << "states, "
        << L::argType(L::types::double_pt)
        << "algebraic"
        << L::argListCl()  << std::endl
        << L::funBodyOp() << std::endl
        << L::funBodyCl();
    return oss.str();
}

void Generator::GeneratorImpl::findInitialValues(ComponentPtr c)
{
    for (std::size_t i = 0; i < c->variableCount(); i++)
    {
        auto v = c->getVariable(i);
        if (v->getName() != mVoi)
        {
            mInitialValues[v->getName()] = std::stod(v->getInitialValue());
        }
    }
}

template<typename L>
std::string Generator::GeneratorImpl::doGenerateCode(ModelPtr m)
{
    ComponentPtr c = m->getComponent(0);

    findVOI(c->getMath());
    findInitialValues(c);
    auto r = parseMathML(c->getMath());
    
    std::string generatedCode;
    std::ostringstream oss(generatedCode);
    oss << generateInitConsts<L>() << std::endl;
    oss << generateComputeRates<L>(r) << std::endl;
    oss << generateComputeVariables<L>() << std::endl;

    mCode = oss.str();
    return mCode;
}

template <typename L>
std::string Generator::generateCode(ModelPtr m)
{
    return mPimpl->doGenerateCode<L>(m);
}

void Generator::writeCodeToFile(std::string filename)
{
    if (mPimpl->mCode == "")
    {
        ErrorPtr err = std::make_shared<Error>();
        err->setDescription("No code was generated yet, you should call "
                "Generator::generateCode before calling this method.");
        addError(err);
        throw CodeNotGenerated();
    }

    std::ofstream output(filename);
    output << mPimpl->mCode;
    output.close();
}

std::shared_ptr<Representable> Generator::GeneratorImpl::parseNode(XmlNodePtr node)
{
    if (node->isType("apply"))
    {
        return parseNode(node->getFirstChild());
    }
    else if (node->isType("plus"))
    {
        auto c = std::make_shared<Addition>();
        auto s = node->getNext();
        c->setArg1(parseNode(s));
        s = s->getNext();
        if (!s->getNext())
        {
            c->setArg2(parseNode(s));
        }
        else
        {
            auto pointer0 = c;
            while (s->getNext())
            {
                auto pointer1 = std::make_shared<Addition>();
                pointer1->setArg1(parseNode(s));
                pointer0->setArg2(pointer1);
                s = s->getNext();
                pointer0 = pointer1;
            }
            pointer0->setArg2(parseNode(s));
        }
        return c;
    }
    else if (node->isType("minus"))
    {
        auto c = std::make_shared<Subtraction>();
        auto s1 = node->getNext();
        c->setArg1(parseNode(s1));
        c->setArg2(parseNode(s1->getNext()));
        return c;
    }
    else if (node->isType("times"))
    {
        auto c = std::make_shared<Multiplication>();
        auto s = node->getNext();
        c->setArg1(parseNode(s));
        s = s->getNext();
        if (!s->getNext())
        {
            c->setArg2(parseNode(s));
        }
        else
        {
            auto pointer0 = c;
            while (s->getNext())
            {
                auto pointer1 = std::make_shared<Multiplication>();
                pointer1->setArg1(parseNode(s));
                pointer0->setArg2(pointer1);
                s = s->getNext();
                pointer0 = pointer1;
            }
            pointer0->setArg2(parseNode(s));
        }
        return c;
    }
    else if (node->isType("divide"))
    {
        auto c = std::make_shared<Division>();
        auto s1 = node->getNext();
        c->setArg1(parseNode(s1));
        c->setArg2(parseNode(s1->getNext()));
        return c;
    }
    else if (node->isType("power"))
    {
        auto c = std::make_shared<Power>();
        auto s1 = node->getNext();
        c->setArg1(parseNode(s1));
        c->setArg2(parseNode(s1->getNext()));
        return c;
    }
    else if (node->isType("sin"))
    {
        auto c = std::make_shared<Sine>();
        c->setArg(parseNode(node->getNext()));
        return c;
    }
    else if (node->isType("cos"))
    {
        auto c = std::make_shared<Cosine>();
        c->setArg(parseNode(node->getNext()));
        return c;
    }
    else if (node->isType("abs"))
    {
        auto c = std::make_shared<AbsoluteValue>();
        c->setArg(parseNode(node->getNext()));
        return c;
    }
    else if (node->isType("ci"))
    {
        auto name = node->getFirstChild()->convertToString();
        auto c = std::make_shared<libcellml::operators::Variable>(name);
        if (name != mVoi &&
                std::find(mStates.begin(), mStates.end(), name) == mStates.end())
        {
            mStates.push_back(name);
        }
        return c;
    }
    else if (node->isType("cn"))
    {
        double value;
        std::istringstream iss(node->getFirstChild()->convertToString());
        iss >> value;
        auto c = std::make_shared<Constant>(value);
        return c;
    }
    else
    {
        ErrorPtr err = std::make_shared<Error>();
        err->setDescription("Found node of type "
                + node->getType() +
                " which is currently not supported by the Generator class.");
        mGenerator->addError(err);
        throw UnknownNode();

        return std::make_shared<Constant>(0);
    }
}

std::shared_ptr<Representable> Generator::GeneratorImpl::parseMathML(std::string math)
{
    XmlDocPtr mathDoc = std::make_shared<XmlDoc>();
    mathDoc->parse(math);

    const XmlNodePtr root = mathDoc->getRootNode();

    XmlNodePtr childNode = root->getFirstChild();
    childNode = childNode->getFirstChild();
    childNode = childNode->getNext();
    childNode = childNode->getNext();

    return parseNode(childNode);
}

void Generator::GeneratorImpl::findVOIHelper(XmlNodePtr node)
{
    if (node->isType("bvar"))
    {
        mVoi = node->getFirstChild()->getFirstChild()->convertToString();
        return;
    }
    else
    {
        if (node->getFirstChild())
        {
            findVOIHelper(node->getFirstChild());
        }
        if (node->getNext())
        {
            findVOIHelper(node->getNext());
        }
    }
}

void Generator::GeneratorImpl::findVOI(std::string math)
{
    XmlDocPtr mathDoc = std::make_shared<XmlDoc>();
    mathDoc->parse(math);

    const XmlNodePtr root = mathDoc->getRootNode();
    XmlNodePtr node = root->getFirstChild();

    findVOIHelper(node);
}

const char * CodeNotGenerated::what () const throw ()
{
    return "No code was generated yet, you should call "
        "Generator::generateCode before calling this method.";
}

const char * UnknownNode::what () const throw ()
{
    return "Found node of unknown type";
}

template std::string Generator::generateCode<CXX>(ModelPtr m);

}
