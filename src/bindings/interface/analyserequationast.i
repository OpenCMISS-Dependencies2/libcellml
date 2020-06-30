%module(package="libcellml") analyserequationast

#define LIBCELLML_EXPORT

%feature("docstring") libcellml::AnalyserEquationAst
"Creates an :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::type
"Returns the :enum:`AnalyserEquationAst::Type` for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::setType
"Sets the :enum:`AnalyserEquationAst::Type` for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::value
"Returns the value string for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::setValue
"Sets the value string for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::variable
"Returns the :class:`Variable` for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::setVariable
"Sets the :class:`Variable` for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::parent
"Returns the :class:`AnalyserEquationAst` parent for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::setParent
"Sets the :class:`AnalyserEquationAst` parent for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::leftChild
"Returns the :class:`AnalyserEquationAst` left child for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::setLeftChild
"Sets the :class:`AnalyserEquationAst` left child for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::rightChild
"Returns the :class:`AnalyserEquationAst` right child for this :class:`AnalyserEquationAst` object.";

%feature("docstring") libcellml::AnalyserEquationAst::setRightChild
"Sets the :class:`AnalyserEquationAst` right child for this :class:`AnalyserEquationAst` object.";

%import "createconstructor.i"

%feature("docstring") libcellml::AnalyserEquationAst
"Creates an :class:`AnalyserEquationAst` object.";

%{
#include "libcellml/analyserequationast.h"
%}

%create_constructor(AnalyserEquationAst)

%include "libcellml/types.h"
%include "libcellml/analyserequationast.h"