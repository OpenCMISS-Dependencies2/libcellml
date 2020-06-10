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

#include "test_utils.h"

#include "gtest/gtest.h"

#include <libcellml>

TEST(Reset, create)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_NE(nullptr, r);

    libcellml::ResetPtr rOrder = libcellml::Reset::create(10);

    EXPECT_EQ(10, rOrder->order());
}

TEST(Reset, order)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_FALSE(r->isOrderSet());
    r->setOrder(1);

    EXPECT_EQ(1, r->order());
    EXPECT_TRUE(r->isOrderSet());

    r->unsetOrder();
    EXPECT_FALSE(r->isOrderSet());
}

TEST(Reset, printResetWithVariable)
{
    const std::string e =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\"/>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();
    libcellml::VariablePtr v = libcellml::Variable::create();

    v->setName("A");

    r->setVariable(v);

    c->addReset(r);

    libcellml::PrinterPtr p = libcellml::Printer::create();

    const std::string a = p->printModel(m);
    EXPECT_EQ(e, a);
}

TEST(Reset, printResetWithoutTestValue)
{
    std::string a;
    const std::string e =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\" name=\"model\">\n"
        "  <component name=\"component\">\n"
        "    <variable name=\"variable1\"/>\n"
        "    <variable name=\"variable2\"/>\n"
        "    <reset variable=\"variable1\" test_variable=\"variable2\" order=\"1\">\n"
        "      <reset_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </reset_value>\n"
        "    </reset>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r1 = libcellml::Reset::create();

    libcellml::VariablePtr v1 = libcellml::Variable::create();
    libcellml::VariablePtr v2 = libcellml::Variable::create();

    m->setName("model");
    c->setName("component");
    v1->setName("variable1");
    v2->setName("variable2");

    r1->setVariable(v1);
    r1->setTestVariable(v2);
    r1->setOrder(1);
    r1->setResetValue(EMPTY_MATH);

    c->addReset(r1);
    c->addVariable(v1);
    c->addVariable(v2);

    libcellml::PrinterPtr p = libcellml::Printer::create();
    a = p->printModel(m);
    EXPECT_EQ(e, a);
}

TEST(Reset, printResetWithOrder)
{
    const std::string e =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset order=\"1\"/>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();

    r->setOrder(1);
    c->addReset(r);

    libcellml::PrinterPtr p = libcellml::Printer::create();

    const std::string a = p->printModel(m);
    EXPECT_EQ(e, a);
}

TEST(Reset, printResetWithNegativeOrder)
{
    const std::string e =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset order=\"-1\"/>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();

    r->setOrder(-1);
    c->addReset(r);

    libcellml::PrinterPtr p = libcellml::Printer::create();

    const std::string a = p->printModel(m);
    EXPECT_EQ(e, a);
}

TEST(Reset, printResetWithOrderAndVariable)
{
    const std::string e =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"B\" order=\"1\"/>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();
    libcellml::VariablePtr v = libcellml::Variable::create();

    v->setName("B");

    r->setVariable(v);
    r->setOrder(1);
    c->addReset(r);

    libcellml::PrinterPtr p = libcellml::Printer::create();

    const std::string a = p->printModel(m);
    EXPECT_EQ(e, a);
}

TEST(Reset, addRemoveResetFromComponentMethods)
{
    std::string a;
    const std::string in = "valid_name";

    const std::string e1 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component name=\"valid_name\">\n"
        "    <variable name=\"variable1\"/>\n"
        "    <variable name=\"variable2\"/>\n"
        "    <reset variable=\"variable1\" test_variable=\"variable2\" order=\"1\">\n"
        "      <test_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </test_value>\n"
        "      <reset_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </reset_value>\n"
        "    </reset>\n"
        "  </component>\n"
        "</model>\n";
    const std::string e2 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component name=\"valid_name\">\n"
        "    <variable name=\"variable1\"/>\n"
        "    <variable name=\"variable2\"/>\n"
        "    <reset variable=\"variable1\" test_variable=\"variable2\" order=\"2\">\n" // only difference is order of reset
        "      <test_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </test_value>\n"
        "      <reset_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </reset_value>\n"
        "    </reset>\n"
        "  </component>\n"
        "</model>\n";
    const std::string e3 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component name=\"valid_name\">\n"
        "    <variable name=\"variable1\"/>\n"
        "    <variable name=\"variable2\"/>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::VariablePtr v1 = libcellml::Variable::create();
    libcellml::VariablePtr v2 = libcellml::Variable::create();
    libcellml::ResetPtr r1 = libcellml::Reset::create();
    libcellml::ResetPtr r2 = libcellml::Reset::create();
    libcellml::ResetPtr r3 = libcellml::Reset::create();
    libcellml::PrinterPtr printer = libcellml::Printer::create();

    c->setName(in);
    v1->setName("variable1");
    v2->setName("variable2");

    r1->setVariable(v1);
    r1->setTestVariable(v2);
    r1->setOrder(1);
    r1->setResetValue(EMPTY_MATH);
    r1->setTestValue(EMPTY_MATH);

    c->addReset(r1);
    c->addVariable(v1);
    c->addVariable(v2);

    a = printer->printModel(m);
    EXPECT_EQ(e1, a);

    // Add another reset
    r2->setVariable(v1);
    r2->setTestVariable(v2);
    r2->setOrder(2);
    r2->setResetValue(EMPTY_MATH);
    r2->setTestValue(EMPTY_MATH);
    c->addReset(r2);

    // Try and add an existing reset to trigger 'false' return
    EXPECT_FALSE(c->addReset(r1));

    // Try and add a nullprt to trigger 'false' return
    EXPECT_FALSE(c->addReset(nullptr));

    // Remove the first one and print the model
    c->removeReset(r1);
    a = printer->printModel(m);
    EXPECT_EQ(e2, a);

    // Remove the second one and print the model
    c->removeReset(0);
    a = printer->printModel(m);
    EXPECT_EQ(e3, a);

    // Add them both back in and use removeAllResets to remove them
    c->addReset(r1);
    c->addReset(r2);
    EXPECT_EQ(size_t(2), c->resetCount());

    c->removeAllResets();
    a = printer->printModel(m);
    EXPECT_EQ(e3, a);

    // Try and remove the ones which don't exist so we trigger the 'false' return statement
    EXPECT_FALSE(c->removeReset(1));
    EXPECT_FALSE(c->removeReset(r1));

    // Coverage for the takeReset function
    c->addReset(r1);
    EXPECT_EQ(size_t(1), c->resetCount());
    auto taken_r1 = c->takeReset(0);
    EXPECT_EQ(r1, taken_r1);
    EXPECT_EQ(size_t(0), c->resetCount());
}

TEST(Reset, resetFromComponentMethod)
{
    const std::string in = "valid_name";
    libcellml::ComponentPtr c = libcellml::Component::create();
    c->setName(in);

    libcellml::ResetPtr r1 = libcellml::Reset::create();
    c->addReset(r1);
    libcellml::ResetPtr r2 = libcellml::Reset::create();
    c->addReset(r2);
    libcellml::ResetPtr r3 = libcellml::Reset::create();
    c->addReset(r3);
    libcellml::ResetPtr r4 = libcellml::Reset::create();
    c->addReset(r4);

    EXPECT_EQ(size_t(4), c->resetCount());

    // Get by index
    libcellml::ResetPtr rMethod1 = c->reset(1);
    EXPECT_EQ(r2.get(), rMethod1.get());

    // Get const by index
    const libcellml::ResetPtr vMethod2 = c->reset(3);
    EXPECT_EQ(r4.get(), vMethod2.get());

    // Get invalid index
    EXPECT_EQ(nullptr, c->reset(7));
}

TEST(Reset, hasResetFromComponentMethod)
{
    const std::string in = "valid_name";
    libcellml::ComponentPtr c = libcellml::Component::create();
    c->setName(in);

    libcellml::ResetPtr r1 = libcellml::Reset::create();
    c->addReset(r1);
    libcellml::ResetPtr r2 = libcellml::Reset::create();
    c->addReset(r2);
    libcellml::ResetPtr r3 = libcellml::Reset::create();

    EXPECT_TRUE(c->hasReset(r2));
    EXPECT_FALSE(c->hasReset(r3));
}

TEST(Reset, printResetWithVariableAndTestVariable)
{
    const std::string e =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\"/>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();

    libcellml::VariablePtr v1 = libcellml::Variable::create();
    libcellml::VariablePtr v2 = libcellml::Variable::create();

    v1->setName("A");
    v2->setName("B");

    r->setVariable(v1);
    r->setTestVariable(v2);
    r->setOrder(1);
    c->addReset(r);

    libcellml::PrinterPtr p = libcellml::Printer::create();

    const std::string a = p->printModel(m);
    EXPECT_EQ(e, a);
}

TEST(Reset, testValueSetClear)
{
    const std::string test1 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\"/>\n"
        "  </component>\n"
        "</model>\n";

    const std::string test2 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\">\n"
        "      <test_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </test_value>\n"
        "    </reset>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();
    libcellml::VariablePtr v1 = libcellml::Variable::create();
    libcellml::VariablePtr v2 = libcellml::Variable::create();
    libcellml::PrinterPtr p = libcellml::Printer::create();

    v1->setName("A");
    v2->setName("B");

    r->setVariable(v1);
    r->setTestVariable(v2);
    r->setOrder(1);
    c->addReset(r);

    const std::string out1 = p->printModel(m);
    EXPECT_EQ(test1, out1);

    // Test setting of test_value block
    r->setTestValue(EMPTY_MATH);
    const std::string out2 = p->printModel(m);
    EXPECT_EQ(test2, out2);

    // Test clearing of test_value block
    r->removeTestValue();
    const std::string out3 = p->printModel(m);
    EXPECT_EQ(test1, out3);
}

TEST(Reset, testValueAppend)
{
    static const std::string firstMaths = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\">\n";
    static const std::string secondMaths = "</math>\n";

    const std::string in =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\"/>\n"
        "  </component>\n"
        "</model>\n";

    const std::string test =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\">\n"
        "      <test_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </test_value>\n"
        "    </reset>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();
    libcellml::VariablePtr v1 = libcellml::Variable::create();
    libcellml::VariablePtr v2 = libcellml::Variable::create();
    libcellml::PrinterPtr p = libcellml::Printer::create();

    v1->setName("A");
    v2->setName("B");

    r->setVariable(v1);
    r->setTestVariable(v2);
    r->setOrder(1);
    c->addReset(r);

    // Test appending of test_value block
    r->appendTestValue(firstMaths);
    r->appendTestValue(secondMaths);

    const std::string out = p->printModel(m);
    EXPECT_EQ(test, out);
}

TEST(Reset, resetValueSetClear)
{
    const std::string test1 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\"/>\n"
        "  </component>\n"
        "</model>\n";

    const std::string test2 =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\">\n"
        "      <reset_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </reset_value>\n"
        "    </reset>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();
    libcellml::VariablePtr v1 = libcellml::Variable::create();
    libcellml::VariablePtr v2 = libcellml::Variable::create();
    libcellml::PrinterPtr p = libcellml::Printer::create();

    v1->setName("A");
    v2->setName("B");

    r->setVariable(v1);
    r->setTestVariable(v2);
    r->setOrder(1);
    c->addReset(r);

    const std::string out1 = p->printModel(m);
    EXPECT_EQ(test1, out1);

    // Test setting of reset_value block
    r->setResetValue(EMPTY_MATH);
    const std::string out2 = p->printModel(m);
    EXPECT_EQ(test2, out2);

    // Test clearing of reset_value block
    r->removeResetValue();
    const std::string out3 = p->printModel(m);
    EXPECT_EQ(test1, out3);
}

TEST(Reset, resetValueAppend)
{
    static const std::string firstMaths = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\">\n";
    static const std::string secondMaths = "</math>\n";

    const std::string test =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<model xmlns=\"http://www.cellml.org/cellml/2.0#\">\n"
        "  <component>\n"
        "    <reset variable=\"A\" test_variable=\"B\" order=\"1\">\n"
        "      <reset_value>\n"
        "        <math xmlns=\"http://www.w3.org/1998/Math/MathML\"/>\n"
        "      </reset_value>\n"
        "    </reset>\n"
        "  </component>\n"
        "</model>\n";

    libcellml::ModelPtr m = createModelWithComponent();
    libcellml::ComponentPtr c = m->component(0);
    libcellml::ResetPtr r = libcellml::Reset::create();
    libcellml::VariablePtr v1 = libcellml::Variable::create();
    libcellml::VariablePtr v2 = libcellml::Variable::create();
    libcellml::PrinterPtr p = libcellml::Printer::create();

    v1->setName("A");
    v2->setName("B");

    r->setVariable(v1);
    r->setTestVariable(v2);
    r->setOrder(1);
    c->addReset(r);

    // Test appending of reset_value block
    r->appendResetValue(firstMaths);
    r->appendResetValue(secondMaths);

    const std::string out = p->printModel(m);
    EXPECT_EQ(test, out);
}

TEST(Reset, resetRemoveTestValue)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_EQ("", r->testValue());

    r->setTestValue("some test condition math");

    EXPECT_EQ("some test condition math", r->testValue());

    r->removeTestValue();

    EXPECT_EQ("", r->testValue());
}

TEST(Reset, resetRemoveResetValue)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_EQ("", r->resetValue());

    r->setResetValue("some reset sort of math");

    EXPECT_EQ("some reset sort of math", r->resetValue());

    r->removeResetValue();

    EXPECT_EQ("", r->resetValue());
}

TEST(Reset, resetSetTestValueId)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_EQ("", r->testValueId());

    r->setTestValueId("id1");

    EXPECT_EQ("id1", r->testValueId());
}

TEST(Reset, resetSetResetValueId)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_EQ("", r->resetValueId());

    r->setResetValueId("id1");

    EXPECT_EQ("id1", r->resetValueId());
}

TEST(Reset, resetRemoveTestValueId)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_EQ("", r->testValueId());

    r->setTestValueId("id1");

    EXPECT_EQ("id1", r->testValueId());

    r->removeTestValueId();

    EXPECT_EQ("", r->testValueId());
}

TEST(Reset, resetRemoveResetValueId)
{
    libcellml::ResetPtr r = libcellml::Reset::create();

    EXPECT_EQ("", r->resetValueId());

    r->setResetValueId("id1");

    EXPECT_EQ("id1", r->resetValueId());

    r->removeResetValueId();

    EXPECT_EQ("", r->resetValueId());
}
