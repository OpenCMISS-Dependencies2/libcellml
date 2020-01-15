%module(package="libcellml") issue

#define LIBCELLML_EXPORT

%include <std_string.i>

%import "createconstructor.i"
%import "types.i"

%feature("docstring") libcellml::Issue
"Base class for errors used with logger derived classes."

%feature("docstring") libcellml::Issue::description
"Get a string description for why this issue was raised.";

%feature("docstring") libcellml::Issue::setDescription
"Sets a string description for why this issue was raised.";

%feature("docstring") libcellml::Issue::kind
"Get the ``kind`` of this issue. If no kind has been set for this issue, will
return Cause::UNDEFINED.";

%feature("docstring") libcellml::Issue::isCause
"Tests if this issue matches the given ``kind``.";

%feature("docstring") libcellml::Issue::setCause
"Sets the ``kind`` of this issue.";

%feature("docstring") libcellml::Issue::rule
"Get the :class:`SpecificationRule` of this issue.";

%feature("docstring") libcellml::Issue::setRule
"Sets the :class:`SpecificationRule` for this issue.";

%feature("docstring") libcellml::Issue::specificationHeading
"Returns the CellML 2.0 Specification heading associated with the
:class:`SpecificationRule` for this issue (empty string if not set).";

%feature("docstring") libcellml::Issue::component
"Returns the :class:`Component` that this issue is relevant to (or ``None``).";

%feature("docstring") libcellml::Issue::setComponent
"Sets the :class:`Component` that this issue is relevant to (``None`` to unset).";

%feature("docstring") libcellml::Issue::importSource
"Returns the :class:`ImportSource` that this issue is relevant to (or ``None``).";

%feature("docstring") libcellml::Issue::setImportSource
"Sets the :class:`ImportSource` that this issue is relevant to (``None`` to unset).";

%feature("docstring") libcellml::Issue::model
"Returns the :class:`Model` that this issue is relevant to (or ``None``).";

%feature("docstring") libcellml::Issue::setModel
"Sets the :class:`Model` that this issue is relevant to (``None`` to unset).";

%feature("docstring") libcellml::Issue::units
"Get the :class:`Units` that this issue is relevant to (or ``None``).";

%feature("docstring") libcellml::Issue::setUnits
"Sets the :class`Units` that this issue is relevant to (``None`` to unset).";

%feature("docstring") libcellml::Issue::variable
"Get the :class:`Variable` that this issue is relevant to (or ``None``).";

%feature("docstring") libcellml::Issue::setVariable
"Sets the :class:`Variable` that this issue is relevant to (``None`` to unset).";

%feature("docstring") libcellml::Issue::reset
"Get the :class:`Reset` that this issue is relevant to (or ``None``).";

%feature("docstring") libcellml::Issue::setReset
"Sets the :class:`Reset` that this issue is relevant to (``None`` to unset).";

%{
#include "libcellml/issue.h"
%}

%create_constructor(Issue)
%extend libcellml::Issue {
    Issue(const ComponentPtr &component) {
        auto ptr = new std::shared_ptr<  libcellml::Issue >(libcellml::Issue::create(component));
        return reinterpret_cast<libcellml::Issue *>(ptr);
    }
    Issue(const ImportSourcePtr &importSource) {
        auto ptr = new std::shared_ptr<  libcellml::Issue >(libcellml::Issue::create(importSource));
        return reinterpret_cast<libcellml::Issue *>(ptr);
    }
    Issue(const ModelPtr &model) {
        auto ptr = new std::shared_ptr<  libcellml::Issue >(libcellml::Issue::create(model));
        return reinterpret_cast<libcellml::Issue *>(ptr);
    }
    Issue(const ResetPtr &reset) {
        auto ptr = new std::shared_ptr<  libcellml::Issue >(libcellml::Issue::create(reset));
        return reinterpret_cast<libcellml::Issue *>(ptr);
    }
    Issue(const UnitsPtr &units) {
        auto ptr = new std::shared_ptr<  libcellml::Issue >(libcellml::Issue::create(units));
        return reinterpret_cast<libcellml::Issue *>(ptr);
    }
    Issue(const VariablePtr &variable) {
        auto ptr = new std::shared_ptr<  libcellml::Issue >(libcellml::Issue::create(variable));
        return reinterpret_cast<libcellml::Issue *>(ptr);
    }
}

%include "libcellml/exportdefinitions.h"
%include "libcellml/specificationrules.h"
%include "libcellml/types.h"
%include "libcellml/issue.h"
