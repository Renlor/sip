/*
 * The PEP 484 type hints generator for SIP.
 *
 * Copyright (c) 2016 Riverbank Computing Limited <info@riverbankcomputing.com>
 *
 * This file is part of SIP.
 *
 * This copy of SIP is licensed for use under the terms of the SIP License
 * Agreement.  See the file LICENSE for more details.
 *
 * This copy of SIP may also used under the terms of the GNU General Public
 * License v2 or v3 as published by the Free Software Foundation which can be
 * found in the files LICENSE-GPL2 and LICENSE-GPL3 included in this package.
 *
 * SIP is supplied WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */


#include <stdio.h>
#include <string.h>

#include "sip.h"


static void pyiCompositeModule(sipSpec *pt, moduleDef *comp_mod, FILE *fp);
static void pyiModule(sipSpec *pt, moduleDef *mod, FILE *fp);
static void pyiTypeHintCode(codeBlockList *thc, FILE *fp);
static void pyiEnums(sipSpec *pt, moduleDef *mod, ifaceFileDef *scope,
        ifaceFileList *defined, int indent, FILE *fp);
static void pyiVars(sipSpec *pt, moduleDef *mod, classDef *scope,
        ifaceFileList *defined, int indent, FILE *fp);
static void pyiClass(sipSpec *pt, moduleDef *mod, classDef *cd,
        ifaceFileList **defined, int indent, FILE *fp);
static void pyiMappedType(sipSpec *pt, moduleDef *mod, mappedTypeDef *mtd,
        ifaceFileList **defined, int indent, FILE *fp);
static void pyiCtor(sipSpec *pt, moduleDef *mod, ctorDef *ct, int overloaded,
        int sec, ifaceFileList *defined, int indent, FILE *fp);
static void pyiCallable(sipSpec *pt, moduleDef *mod, memberDef *md,
        overDef *overloads, int is_method, ifaceFileList *defined, int indent,
        FILE *fp);
static void pyiOverload(sipSpec *pt, moduleDef *mod, overDef *od,
        int overloaded, int is_method, int sec, ifaceFileList *defined,
        int indent, FILE *fp);
static void pyiPythonSignature(sipSpec *pt, moduleDef *mod, signatureDef *sd,
        int need_self, int sec, ifaceFileList *defined, FILE *fp);
static int pyiArgument(sipSpec *pt, moduleDef *mod, argDef *ad, int arg_nr,
        int out, int need_comma, int sec, int names, int defaults,
        ifaceFileList *defined, FILE *fp);
static void pyiType(sipSpec *pt, moduleDef *mod, argDef *ad, int out, int sec,
        ifaceFileList *defined, FILE *fp);
static void pyiTypeHint(sipSpec *pt, moduleDef *mod, typeHintDef *thd, int out,
        ifaceFileList *defined, FILE *fp);
static void prIndent(int indent, FILE *fp);
static int separate(int first, int indent, FILE *fp);
static void prClassRef(sipSpec *pt, classDef *cd, int out, moduleDef *mod,
        ifaceFileList *defined, FILE *fp);
static void prEnumRef(enumDef *ed, moduleDef *mod, ifaceFileList *defined,
        FILE *fp);
static void prMappedTypeRef(sipSpec *pt, mappedTypeDef *mtd, int out,
        moduleDef *mod, ifaceFileList *defined, FILE *fp);
static int isDefined(ifaceFileDef *iff, classDef *cd, moduleDef *mod,
        ifaceFileList *defined);
static int inIfaceFileList(ifaceFileDef *iff, ifaceFileList *defined);
static int hasImplicitOverloads(signatureDef *sd);
static void parseTypeHint(sipSpec *pt, typeHintDef *thd);
static void lookupType(sipSpec *pt, char *name, argDef *ad);
static enumDef *lookupEnum(sipSpec *pt, const char *name, classDef *scope_cd,
        mappedTypeDef *scope_mtd);
static mappedTypeDef *lookupMappedType(sipSpec *pt, const char *name);
static classDef *lookupClass(sipSpec *pt, const char *name,
        classDef *scope_cd);
static int inDefaultAPI(sipSpec *pt, apiVersionRangeDef *range);
static classDef *getClassImplementation(sipSpec *pt, classDef *cd);
static mappedTypeDef *getMappedTypeImplementation(sipSpec *pt,
        mappedTypeDef *mtd);


/*
 * Generate the .pyi file.
 */
void generateTypeHints(sipSpec *pt, moduleDef *mod, const char *pyiFile)
{
    FILE *fp;

    /* Generate the file. */
    if ((fp = fopen(pyiFile, "w")) == NULL)
        fatal("Unable to create file \"%s\"\n", pyiFile);

    /* Write the header. */
    fprintf(fp,
"# The PEP 484 type hints stub file for the %s module.\n"
"#\n"
"# Generated by SIP %s\n"
        , mod->name
        , sipVersion);

    prCopying(fp, mod, "#");

    fprintf(fp,
"\n"
"\n"
        );

    if (isComposite(mod))
        pyiCompositeModule(pt, mod, fp);
    else
        pyiModule(pt, mod, fp);

    fclose(fp);
}


/*
 * Generate the type hints for a composite module.
 */
static void pyiCompositeModule(sipSpec *pt, moduleDef *comp_mod, FILE *fp)
{
    moduleDef *mod;

    for (mod = pt->modules; mod != NULL; mod = mod->next)
        if (mod->container == comp_mod)
            fprintf(fp, "from %s import *\n", mod->fullname->text);
}


/*
 * Generate the type hints for an ordinary module.
 */
static void pyiModule(sipSpec *pt, moduleDef *mod, FILE *fp)
{
    char *cp;
    int first;
    memberDef *md;
    classDef *cd;
    mappedTypeDef *mtd;
    ifaceFileList *defined;
    moduleListDef *mld;

    /*
     * Generate the imports. Note that we assume the super-types are the
     * standard SIP ones.
     */
    fprintf(fp,
"from typing import (Any, Callable, Dict, Iterator, List, Mapping, Optional,\n"
"        overload, Sequence, Set, Tuple, TypeVar, Union)\n"
"\n"
"import sip\n"
        );

    first = TRUE;

    for (mld = mod->imports; mld != NULL; mld = mld->next)
    {
        /* We lie about the indent because we only want one blank line. */
        first = separate(first, 1, fp);

        if ((cp = strrchr(mld->module->fullname->text, '.')) == NULL)
        {
            fprintf(fp, "import %s\n", mld->module->name);
        }
        else
        {
            *cp = '\0';
            fprintf(fp, "from %s import %s\n", mld->module->fullname->text,
                    mld->module->name);
            *cp = '.';
        }
    }

    fprintf(fp,
"\n"
"\n"
"# PEP 484 doesn't have support for these types.\n"
"PY_BUFFER_T = TypeVar('PY_BUFFER_T')\n"
"PY_SLICE_T = TypeVar('PY_SLICE_T')\n"
"PY_TYPE_T = TypeVar('PY_TYPE_T')\n"
        );

    if (pluginPyQt4(pt) || pluginPyQt5(pt))
        fprintf(fp,
"\n"
"# Support for old-style signals and slots.\n"
"SIGNAL_T = TypeVar('SIGNAL_T')\n"
"SLOT_T = TypeVar('SLOT_T')\n"
"SLOT_SIGNAL_T = Union[SLOT_T, SIGNAL_T]\n"
            );

    /*
     * Generate any exported type hint code and any module-specific type hint
     * code.
     */
    if (pt->exptypehintcode != NULL)
        pyiTypeHintCode(pt->exptypehintcode, fp);

    if (mod->typehintcode != NULL)
        pyiTypeHintCode(mod->typehintcode, fp);

    // FIXME: Have a NoTypeHint function anno that suppresses the generation of
    // any type hint - on the assumption that it will be implemented in
    // handwritten code.
    // FIXME: Types with slot extenders cannot have predictable arguments so
    // need to specify Any and NoTypeHint for the overloads in the originating
    // module. (QDataStream, QTextStream)  Apply NoTypeHint automatically in
    // the sub-modules.
    // FIXME: Re-implement the docstring support.
    // %ConvertToTypeCode.

    /* Generate the types - global enums must be first. */
    pyiEnums(pt, mod, NULL, NULL, 0, fp);

    defined = NULL;

    for (cd = pt->classes; cd != NULL; cd = cd->next)
    {
        classDef *impl;

        if (cd->iff->module != mod)
            continue;

        if (isExternal(cd))
            continue;

        if (cd->no_typehint)
            continue;

        /* Only handle non-nested classes here. */
        if (cd->ecd != NULL)
            continue;

        impl = getClassImplementation(pt, cd);

        if (impl != NULL)
            pyiClass(pt, mod, impl, &defined, 0, fp);
    }

    for (mtd = pt->mappedtypes; mtd != NULL; mtd = mtd->next)
    {
        mappedTypeDef *impl;

        if (mtd->iff->module != mod)
            continue;

        impl = getMappedTypeImplementation(pt, mtd);

        if (impl != NULL && impl->pyname != NULL)
            pyiMappedType(pt, mod, impl, &defined, 0, fp);
    }

    pyiVars(pt, mod, NULL, defined, 0, fp);

    first = TRUE;

    for (md = mod->othfuncs; md != NULL; md = md->next)
        if (md->slot == no_slot)
        {
            first = separate(first, 0, fp);

            pyiCallable(pt, mod, md, mod->overs, FALSE, defined, 0, fp);
        }
}


/*
 * Generate handwritten type hint code.
 */
static void pyiTypeHintCode(codeBlockList *thc, FILE *fp)
{
    fprintf(fp, "\n");

    while (thc != NULL)
    {
        fprintf(fp, "%s", thc->block->frag);
        thc = thc->next;
    }
}


/*
 * Generate the type hints for a class.
 */
static void pyiClass(sipSpec *pt, moduleDef *mod, classDef *cd,
        ifaceFileList **defined, int indent, FILE *fp)
{
    int first, no_body, nr_overloads;
    classDef *nested;
    ctorDef *ct;
    memberDef *md;

    separate(TRUE, indent, fp);
    prIndent(indent, fp);
    fprintf(fp, "class %s(", cd->pyname->text);

    if (cd->supers != NULL)
    {
        classList *cl;

        for (cl = cd->supers; cl != NULL; cl = cl->next)
        {
            if (cl != cd->supers)
                fprintf(fp, ", ");

            prClassRef(pt, cl->cd, TRUE, mod, *defined, fp);
        }
    }
    else if (cd->supertype != NULL)
    {
        fprintf(fp, "%s", cd->supertype->text);
    }
    else if (cd->iff->type == namespace_iface)
    {
        fprintf(fp, "sip.simplewrapper");
    }
    else
    {
        fprintf(fp, "sip.wrapper");
    }

    /* See if there is anything in the class body. */
    nr_overloads = 0;

    for (ct = cd->ctors; ct != NULL; ct = ct->next)
    {
        if (isPrivateCtor(ct))
            continue;

        if (ct->no_typehint)
            continue;

        if (!inDefaultAPI(pt, ct->api_range))
            continue;

        ++nr_overloads;
    }

    no_body = (nr_overloads == 0);

    if (no_body)
    {
        overDef *od;

        for (od = cd->overs; od != NULL; od = od->next)
        {
            if (isPrivate(od))
                continue;

            if (od->no_typehint)
                continue;

            if (inDefaultAPI(pt, od->api_range))
            {
                no_body = FALSE;
                break;
            }
        }
    }

    if (no_body)
    {
        enumDef *ed;

        for (ed = pt->enums; ed != NULL; ed = ed->next)
        {
            if (ed->no_typehint)
                continue;

            if (ed->ecd == cd)
            {
                no_body = FALSE;
                break;
            }
        }
    }

    if (no_body)
    {
        for (nested = pt->classes; nested != NULL; nested = nested->next)
        {
            if (nested->no_typehint)
                continue;

            if (nested->ecd == cd)
            {
                no_body = FALSE;
                break;
            }
        }
    }

    if (no_body)
    {
        varDef *vd;

        for (vd = pt->vars; vd != NULL; vd = vd->next)
        {
            if (vd->no_typehint)
                continue;

            if (vd->ecd == cd)
            {
                no_body = FALSE;
                break;
            }
        }
    }

    fprintf(fp, "):%s\n", (no_body ? " ..." : ""));

    ++indent;

    pyiEnums(pt, mod, cd->iff, *defined, indent, fp);

    /* Handle any nested classes. */
    for (nested = pt->classes; nested != NULL; nested = nested->next)
    {
        classDef *impl = getClassImplementation(pt, nested);

        if (impl != NULL && impl->ecd == cd && !impl->no_typehint)
            pyiClass(pt, mod, impl, defined, indent, fp);
    }

    pyiVars(pt, mod, cd, *defined, indent, fp);

    first = TRUE;

    for (ct = cd->ctors; ct != NULL; ct = ct->next)
    {
        int implicit_overloads, overloaded;

        if (isPrivateCtor(ct))
            continue;

        if (ct->no_typehint)
            continue;

        if (!inDefaultAPI(pt, ct->api_range))
            continue;

        implicit_overloads = hasImplicitOverloads(&ct->pysig);
        overloaded = (implicit_overloads || nr_overloads > 1);

        first = separate(first, indent, fp);

        pyiCtor(pt, mod, ct, overloaded, FALSE, *defined, indent, fp);

        if (implicit_overloads)
            pyiCtor(pt, mod, ct, overloaded, TRUE, *defined, indent, fp);
    }

    first = TRUE;

    for (md = cd->members; md != NULL; md = md->next)
    {
        first = separate(first, indent, fp);

        pyiCallable(pt, mod, md, cd->overs, TRUE, *defined, indent, fp);
    }

    /*
     * Keep track of what has been defined so that forward references are no
     * longer required.
     */
    appendToIfaceFileList(defined, cd->iff);
}


/*
 * Generate the type hints for a mapped type.
 */
static void pyiMappedType(sipSpec *pt, moduleDef *mod, mappedTypeDef *mtd,
        ifaceFileList **defined, int indent, FILE *fp)
{
    int first, no_body;
    memberDef *md;

    /* See if there is anything in the mapped type body. */
    no_body = (mtd->members == NULL);

    if (no_body)
    {
        enumDef *ed;

        for (ed = pt->enums; ed != NULL; ed = ed->next)
        {
            if (ed->no_typehint)
                continue;

            if (ed->emtd == mtd)
            {
                no_body = FALSE;
                break;
            }
        }
    }

    if (!no_body)
    {
        separate(TRUE, indent, fp);
        prIndent(indent, fp);
        fprintf(fp, "class %s(sip.wrapper):\n", mtd->pyname->text);

        ++indent;

        pyiEnums(pt, mod, mtd->iff, *defined, indent, fp);

        first = TRUE;

        for (md = mtd->members; md != NULL; md = md->next)
        {
            first = separate(first, indent, fp);

            pyiCallable(pt, mod, md, mtd->overs, TRUE, *defined, indent, fp);
        }
    }

    /*
     * Keep track of what has been defined so that forward references are no
     * longer required.
     */
    appendToIfaceFileList(defined, mtd->iff);
}


/*
 * Generate an ctor type hint.
 */
static void pyiCtor(sipSpec *pt, moduleDef *mod, ctorDef *ct, int overloaded,
        int sec, ifaceFileList *defined, int indent, FILE *fp)
{
    int a;

    if (overloaded)
    {
        prIndent(indent, fp);
        fprintf(fp, "@overload\n");
    }

    prIndent(indent, fp);
    fprintf(fp, "def __init__(self");

    for (a = 0; a < ct->pysig.nrArgs; ++a)
        pyiArgument(pt, mod, &ct->pysig.args[a], a, FALSE, TRUE, sec, TRUE,
                TRUE, defined, fp);

    fprintf(fp, ") -> None: ...\n");
}


/*
 * Generate the APIs for all the enums in a scope.
 */
static void pyiEnums(sipSpec *pt, moduleDef *mod, ifaceFileDef *scope,
        ifaceFileList *defined, int indent, FILE *fp)
{
    enumDef *ed;

    for (ed = pt->enums; ed != NULL; ed = ed->next)
    {
        enumMemberDef *emd;

        if (ed->module != mod)
            continue;

        if (ed->no_typehint)
            continue;

        if (scope != NULL)
        {
            if ((ed->ecd == NULL || ed->ecd->iff != scope) && (ed->emtd == NULL || ed->emtd->iff != scope))
                continue;
        }
        else if (ed->ecd != NULL || ed->emtd != NULL)
        {
            continue;
        }

        separate(TRUE, indent, fp);

        if (ed->pyname != NULL)
        {
            prIndent(indent, fp);
            fprintf(fp, "class %s(int): ...\n", ed->pyname->text);
        }

        for (emd = ed->members; emd != NULL; emd = emd->next)
        {
            if (emd->no_typehint)
                continue;

            prIndent(indent, fp);
            fprintf(fp, "%s = ... # type: ", emd->pyname->text);

            if (ed->pyname != NULL)
                prEnumRef(ed, mod, defined, fp);
            else
                fprintf(fp, "int");

            fprintf(fp, "\n");
        }
    }
}


/*
 * Generate the APIs for all the variables in a scope.
 */
static void pyiVars(sipSpec *pt, moduleDef *mod, classDef *scope,
        ifaceFileList *defined, int indent, FILE *fp)
{
    int first = TRUE;
    varDef *vd;

    for (vd = pt->vars; vd != NULL; vd = vd->next)
    {
        if (vd->module != mod)
            continue;

        if (vd->ecd != scope)
            continue;

        if (vd->no_typehint)
            continue;

        first = separate(first, indent, fp);

        prIndent(indent, fp);
        fprintf(fp, "%s = ... # type: ", vd->pyname->text);
        pyiType(pt, mod, &vd->type, FALSE, FALSE, defined, fp);
        fprintf(fp, "\n");
    }
}


/*
 * Generate the type hints for a callable.
 */
static void pyiCallable(sipSpec *pt, moduleDef *mod, memberDef *md,
        overDef *overloads, int is_method, ifaceFileList *defined, int indent,
        FILE *fp)
{
    int nr_overloads;
    overDef *od;

    /* Count the number of overloads. */
    nr_overloads = 0;

    for (od = overloads; od != NULL; od = od->next)
    {
        if (isPrivate(od))
            continue;

        if (od->common != md)
            continue;

        if (od->no_typehint)
            continue;

        if (!inDefaultAPI(pt, od->api_range))
            continue;

        ++nr_overloads;
    }

    /* Handle each overload. */
    for (od = overloads; od != NULL; od = od->next)
    {
        int implicit_overloads, overloaded;

        if (isPrivate(od))
            continue;

        if (od->common != md)
            continue;

        if (od->no_typehint)
            continue;

        if (!inDefaultAPI(pt, od->api_range))
            continue;

        implicit_overloads = hasImplicitOverloads(&od->pysig);
        overloaded = (implicit_overloads || nr_overloads > 1);

        pyiOverload(pt, mod, od, overloaded, is_method, FALSE, defined, indent,
                fp);

        if (implicit_overloads)
            pyiOverload(pt, mod, od, overloaded, is_method, TRUE, defined,
                    indent, fp);
    }
}


/*
 * Generate a single API overload.
 */
static void pyiOverload(sipSpec *pt, moduleDef *mod, overDef *od,
        int overloaded, int is_method, int sec, ifaceFileList *defined,
        int indent, FILE *fp)
{
    int need_self;

    if (overloaded)
    {
        prIndent(indent, fp);
        fprintf(fp, "@overload\n");
    }

    if (is_method && isStatic(od))
    {
        prIndent(indent, fp);
        fprintf(fp, "@staticmethod\n");
    }

    prIndent(indent, fp);
    fprintf(fp, "def %s", od->common->pyname->text);

    need_self = (is_method && !isStatic(od));

    pyiPythonSignature(pt, mod, &od->pysig, need_self, sec, defined, fp);

    fprintf(fp, ": ...\n");
}


/*
 * Generate a Python argument.
 */
static int pyiArgument(sipSpec *pt, moduleDef *mod, argDef *ad, int arg_nr,
        int out, int need_comma, int sec, int names, int defaults,
        ifaceFileList *defined, FILE *fp)
{
    int optional;

    if (isArraySize(ad))
        return need_comma;

    if (sec && (ad->atype == slotcon_type || ad->atype == slotdis_type))
        return need_comma;

    if (need_comma)
        fprintf(fp, ", ");

    if (names && ad->atype != ellipsis_type)
    {
        if (ad->name != NULL)
            fprintf(fp, "%s%s: ", ad->name->text,
                    (isPyKeyword(ad->name->text) ? "_" : ""));
        else
            fprintf(fp, "a%d: ", arg_nr);
    }

    optional = (defaults && ad->defval && !out);

    if (optional)
        fprintf(fp, "Optional[");

    pyiType(pt, mod, ad, out, sec, defined, fp);

    if (names && ad->atype == ellipsis_type)
    {
        if (ad->name != NULL)
            fprintf(fp, "%s%s", ad->name->text,
                    (isPyKeyword(ad->name->text) ? "_" : ""));
        else
            fprintf(fp, "a%d", arg_nr);
    }

    if (optional)
        fprintf(fp, "]");

    return TRUE;
}


/*
 * Generate the Python representation of a type.
 */
static void pyiType(sipSpec *pt, moduleDef *mod, argDef *ad, int out, int sec,
        ifaceFileList *defined, FILE *fp)
{
    const char *type_name;
    typeHintDef *thd;

    /* Use any explicit type hint. */
    thd = (out ? ad->typehint_out : ad->typehint_in);

    if (thd != NULL)
    {
        pyiTypeHint(pt, mod, thd, out, defined, fp);
        return;
    }

    /* For classes and mapped types we need the default implementation. */
    if (ad->atype == class_type || ad->atype == mapped_type)
    {
        classDef *cd = ad->u.cd;
        mappedTypeDef *mtd = ad->u.mtd;

        getDefaultImplementation(pt, ad->atype, &cd, &mtd);

        if (cd != NULL)
        {
            prClassRef(pt, cd, out, mod, defined, fp);
        }
        else if (mtd != NULL)
        {
            prMappedTypeRef(pt, mtd, out, mod, defined, fp);
        }
        else
        {
            /*
             * This should never happen as it should have been picked up when
             * generating code - but maybe we haven't been asked to generate
             * code.
             */
            fprintf(fp, "Any");
        }

        return;
    }

    type_name = NULL;

    switch (ad->atype)
    {
    case enum_type:
        if (ad->u.ed->pyname != NULL)
            prEnumRef(ad->u.ed, mod, defined, fp);
        else
            type_name = "int";

        break;

    case capsule_type:
        type_name = scopedNameTail(ad->u.cap);
        break;

    case struct_type:
    case void_type:
        type_name = "sip.voidptr";
        break;

    case signal_type:
        type_name = "SIGNAL_T";
        break;

    case slot_type:
        type_name = "SLOT_SIGNAL_T";
        break;

    case rxcon_type:
    case rxdis_type:
        if (sec)
        {
            type_name = "Callable";
        }
        else
        {
            /* The class should always be found. */
            if (pt->qobject_cd != NULL)
                prClassRef(pt, pt->qobject_cd, out, mod, defined, fp);
            else
                type_name = "Any";
        }

        break;

    case qobject_type:
        type_name = "QObject";
        break;

    case ustring_type:
    case string_type:
    case sstring_type:
    case wstring_type:
    case ascii_string_type:
    case latin1_string_type:
    case utf8_string_type:
        type_name = "str";
        break;

    case byte_type:
    case sbyte_type:
    case ubyte_type:
    case ushort_type:
    case uint_type:
    case long_type:
    case longlong_type:
    case ulong_type:
    case ulonglong_type:
    case short_type:
    case int_type:
    case cint_type:
    case ssize_type:
        type_name = "int";
        break;

    case float_type:
    case cfloat_type:
    case double_type:
    case cdouble_type:
        type_name = "float";
        break;

    case bool_type:
    case cbool_type:
        type_name = "bool";
        break;

    case pyobject_type:
        type_name = "Any";
        break;

    case pytuple_type:
        type_name = "Tuple";
        break;

    case pylist_type:
        type_name = "List";
        break;

    case pydict_type:
        type_name = "Dict";
        break;

    case pycallable_type:
        type_name = "Callable";
        break;

    case pyslice_type:
        type_name = "PY_SLICE_T";
        break;

    case pytype_type:
        type_name = "PY_TYPE_T";
        break;

    case pybuffer_type:
        type_name = "PY_BUFFER_T";
        break;

    case ellipsis_type:
        type_name = "*";
        break;

    case slotcon_type:
    case anyslot_type:
        type_name = "SLOT_T";
        break;

    default:
        type_name = "Any";
    }

    if (type_name != NULL)
        fprintf(fp, "%s", type_name);
}


/*
 * Generate a scoped Python name.
 */
void prScopedPythonName(FILE *fp, classDef *scope, const char *pyname)
{
    if (scope != NULL)
    {
        prScopedPythonName(fp, scope->ecd, NULL);
        fprintf(fp, "%s.", scope->pyname->text);
    }

    if (pyname != NULL)
        fprintf(fp, "%s", pyname);
}


/*
 * Generate a Python signature.
 */
static void pyiPythonSignature(sipSpec *pt, moduleDef *mod, signatureDef *sd,
        int need_self, int sec, ifaceFileList *defined, FILE *fp)
{
    const char *type_name;
    int need_comma, is_res, nr_out, a;

    if (need_self)
    {
        fprintf(fp, "(self");
        need_comma = TRUE;
    }
    else
    {
        fprintf(fp, "(");
        need_comma = FALSE;
    }

    nr_out = 0;

    for (a = 0; a < sd->nrArgs; ++a)
    {
        argDef *ad = &sd->args[a];

        if (isOutArg(ad))
            ++nr_out;

        if (!isInArg(ad))
            continue;

        need_comma = pyiArgument(pt, mod, ad, a, FALSE, need_comma, sec, TRUE,
                TRUE, defined, fp);
    }

    fprintf(fp, ")");

    if (sd->result.typehint_out != NULL)
        type_name = sd->result.typehint_out->raw_hint;
    else
        type_name = NULL;

    is_res = !((sd->result.atype == void_type && sd->result.nrderefs == 0) ||
            (type_name != NULL && type_name[0] == '\0'));

    if (is_res || nr_out > 0)
    {
        fprintf(fp, " -> ");

        if ((is_res && nr_out > 0) || nr_out > 1)
            fprintf(fp, "Tuple[");

        if (is_res)
            need_comma = pyiArgument(pt, mod, &sd->result, -1, TRUE, FALSE,
                    sec, FALSE, FALSE, defined, fp);
        else
            need_comma = FALSE;

        for (a = 0; a < sd->nrArgs; ++a)
        {
            argDef *ad = &sd->args[a];

            if (isOutArg(ad))
                /* We don't want the name in the result tuple. */
                need_comma = pyiArgument(pt, mod, ad, -1, TRUE, need_comma,
                        sec, FALSE, FALSE, defined, fp);
        }

        if ((is_res && nr_out > 0) || nr_out > 1)
            fprintf(fp, "]");
    }
    else
    {
        fprintf(fp, " -> None");
    }
}


/*
 * Generate the required indentation.
 */
static void prIndent(int indent, FILE *fp)
{
    while (indent--)
        fprintf(fp, "    ");
}


/*
 * Generate a newline if not already done.
 */
static int separate(int first, int indent, FILE *fp)
{
    if (first)
        fprintf(fp, (indent ? "\n" : "\n\n"));

    return FALSE;
}


/*
 * Generate a class reference, including its owning module if necessary and
 * handling forward references if necessary.
 */
static void prClassRef(sipSpec *pt, classDef *cd, int out, moduleDef *mod,
        ifaceFileList *defined, FILE *fp)
{
    typeHintDef *thd;

    thd = (out ? NULL : cd->typehint_in);

    if (thd != NULL && !isRecursing(cd->iff))
    {
        setRecursing(cd->iff);
        pyiTypeHint(pt, mod, cd->typehint_in, out, defined, fp);
        resetRecursing(cd->iff);
    }
    else
    {
        int is_defined = isDefined(cd->iff, cd->ecd, mod, defined);

        if (!is_defined)
            fprintf(fp, "'");

        if (cd->iff->module != mod)
            fprintf(fp, "%s.", cd->iff->module->name);

        prScopedPythonName(fp, cd->ecd, cd->pyname->text);

        if (!is_defined)
            fprintf(fp, "'");
    }
}


/*
 * Generate an enum reference, including its owning module if necessary and
 * handling forward references if necessary.
 */
static void prEnumRef(enumDef *ed, moduleDef *mod, ifaceFileList *defined,
        FILE *fp)
{
    int is_defined = (ed->ecd != NULL && isDefined(ed->ecd->iff, ed->ecd->ecd, mod, defined));

    if (!is_defined)
        fprintf(fp, "'");

    if (ed->module != mod)
        fprintf(fp, "%s.", ed->module->name);

    prScopedPythonName(fp, ed->ecd, ed->pyname->text);

    if (!is_defined)
        fprintf(fp, "'");
}


/*
 * Generate a mapped type reference, including its owning module if necessary
 * and handling forward references if necessary.
 */
static void prMappedTypeRef(sipSpec *pt, mappedTypeDef *mtd, int out,
        moduleDef *mod, ifaceFileList *defined, FILE *fp)
{
    typeHintDef *thd;

    thd = (out ? mtd->typehint_out : mtd->typehint_in);

    if (thd != NULL && !isRecursing(mtd->iff))
    {
        setRecursing(mtd->iff);
        pyiTypeHint(pt, mod, thd, out, defined, fp);
        resetRecursing(mtd->iff);
    }
    else if (mtd->pyname != NULL)
    {
        int is_defined = isDefined(mtd->iff, NULL, mod, defined);

        if (mtd->iff->module != mod)
            fprintf(fp, "%s.", mtd->iff->module->name);

        if (!is_defined)
            fprintf(fp, "'");

        fprintf(fp, "%s", mtd->pyname->text);

        if (!is_defined)
            fprintf(fp, "'");
    }
    else
    {
        fprintf(fp, "Any");
    }
}


/*
 * Check if a type has been defined.
 */
static int isDefined(ifaceFileDef *iff, classDef *scope, moduleDef *mod,
        ifaceFileList *defined)
{
    /* A type in another module would have been imported. */
    if (iff->module != mod)
        return TRUE;

    if (!inIfaceFileList(iff, defined))
        return FALSE;

    /* Check all enclosing scopes have been defined as well. */
    while (scope != NULL)
    {
        if (!inIfaceFileList(scope->iff, defined))
            return FALSE;

        scope = scope->ecd;
    }

    return TRUE;
}


/*
 * Check if an interface file appears in a list of them.
 */
static int inIfaceFileList(ifaceFileDef *iff, ifaceFileList *defined)
{
    while (defined != NULL)
    {
        if (defined->iff == iff)
            return TRUE;

        defined = defined->next;
    }

    return FALSE;
}


/*
 * See if a signature has implicit overloads.
 */
static int hasImplicitOverloads(signatureDef *sd)
{
    int a;

    for (a = 0; a < sd->nrArgs; ++a)
    {
        argDef *ad = &sd->args[a];

        if (!isInArg(ad))
            continue;

        if (ad->atype == rxcon_type || ad->atype == rxdis_type)
            return TRUE;
    }

    return FALSE;
}


/*
 * Create a new type hint for a raw string.
 */
typeHintDef *newTypeHint(char *raw_hint)
{
    typeHintDef *thd = sipMalloc(sizeof (typeHintDef));

    thd->needs_parsing = TRUE;
    thd->raw_hint = raw_hint;

    return thd;
}


/*
 * Generate a type hint from a /TypeHint/ annotation.
 */
static void pyiTypeHint(sipSpec *pt, moduleDef *mod, typeHintDef *thd, int out,
        ifaceFileList *defined, FILE *fp)
{
    if (thd->needs_parsing)
        parseTypeHint(pt, thd);

    if (thd->sections != NULL)
    {
        typeHintSection *ths;

        for (ths = thd->sections; ths != NULL; ths = ths->next)
        {
            if (ths->before != NULL)
                fprintf(fp, "%s", ths->before);

            if (ths->type.atype == class_type)
                prClassRef(pt, ths->type.u.cd, out, mod, defined, fp);
            else if (ths->type.atype == enum_type)
                prEnumRef(ths->type.u.ed, mod, defined, fp);
            else
                prMappedTypeRef(pt, ths->type.u.mtd, out, mod, defined, fp);

            if (ths->after != NULL)
                fprintf(fp, "%s", ths->after);
        }
    }
    else
    {
        fprintf(fp, "%s", thd->raw_hint);
    }
}


/*
 * Parse a type hint and update its status accordingly.
 */
static void parseTypeHint(sipSpec *pt, typeHintDef *thd)
{
    char *cp, *before;
    typeHintSection *tail;

    /* No matter what happends we don't do this again. */
    thd->needs_parsing = FALSE;

    /* Parse each section. */
    tail = NULL;
    before = thd->raw_hint;

    /* Ignore leading spaces. */
    for (cp = before; *cp == ' '; ++cp)
        ;

    while (*cp != '\0')
    {
        char *ep, *tp, saved;
        argDef type;

        /* Find the end of a potential type. */
        for (ep = cp; *ep != '\0' && *ep != ' ' && *ep != ']' && *ep != ','; ++ep)
            ;

        /* Find the beginning of the potential type. */
        for (tp = ep - 1; tp >= cp && *tp != ' ' && *tp != '[' && *tp != ','; --tp)
            ;

        ++tp;

        /* Isolate the potential type. */
        saved = *ep;
        *ep = '\0';

        /* Search for the type. */
        lookupType(pt, tp, &type);

        *ep = saved;

        if (type.atype != no_type)
        {
            typeHintSection *ths = sipMalloc(sizeof (typeHintSection));

            *tp = '\0';
            ths->before = before;

            ths->type = type;

            if (tail != NULL)
                tail->next = ths;
            else
                thd->sections = ths;

            tail = ths;

            /* Move past the parsed type. */
            before = ep;
        }

        /*
         * Start the next lookup from the end of this one (after the character
         * that terminated it) and skipping any additional non-significant
         * characters.
         */
        for (cp = ep; *cp != '\0'; ++cp)
            if (strchr(" [],", *cp) == NULL)
                break;
    }

    /* See if there is trailing text. */
    if (*before != '\0' && tail != NULL)
        tail->after = before;
}


/*
 * Look up a qualified Python type.
 */
static void lookupType(sipSpec *pt, char *name, argDef *ad)
{
    char *ep;
    classDef *scope_cd;
    mappedTypeDef *scope_mtd;

    /* Start searching at the global level. */
    scope_cd = NULL;
    scope_mtd = NULL;

    ep = NULL;

    while (name != '\0')
    {
        enumDef *ed;

        /* Isolate the next part of the name. */
        if ((ep = strchr(name, '.')) != NULL)
            *ep = '\0';

        /* See if it's an enum. */
        if ((ed = lookupEnum(pt, name, scope_cd, scope_mtd)) != NULL)
        {
            /* Make sure we have used the whole name. */
            if (ep == NULL)
            {
                ad->atype = enum_type;
                ad->u.ed = ed;

                return;
            }

            /* There is some left so the whole lookup has failed. */
            break;
        }

        /*
         * If we have a mapped type scope then we must be looking for an enum,
         * which we have failed to find.
         */
        if (scope_mtd != NULL)
            break;

        if (scope_cd == NULL)
        {
            mappedTypeDef *mtd;

            /*
             * We are looking at the global level, so see if it is a mapped
             * type.
             */
            if ((mtd = lookupMappedType(pt, name)) != NULL)
            {
                /*
                 * If we have used the whole name then the lookup has
                 * succeeded.
                 */
                if (ep == NULL)
                {
                    ad->atype = mapped_type;
                    ad->u.mtd = mtd;

                    return;
                }

                /* Otherwise this is the scope for the next part. */
                scope_mtd = mtd;
            }
        }

        if (scope_mtd == NULL)
        {
            classDef *cd;

            /* If we get here then it must be a class. */
            if ((cd = lookupClass(pt, name, scope_cd)) == NULL)
                break;

            /* If we have used the whole name then the lookup has succeeded. */
            if (ep == NULL)
            {
                ad->atype = class_type;
                ad->u.cd = cd;

                return;
            }

            /* Otherwise this is the scope for the next part. */
            scope_cd = cd;
        }

        /* If we have run out of name then the lookup has failed. */
        if (ep == NULL)
            break;

        /* Repair the name and go on to the next part. */
        *ep++ = '.';
        name = ep;
    }

    /* Repair the name. */
    if (ep != NULL)
        *ep = '.';

    /* Nothing was found. */
    ad->atype = no_type;
}


/*
 * Lookup an enum.
 */
static enumDef *lookupEnum(sipSpec *pt, const char *name, classDef *scope_cd,
        mappedTypeDef *scope_mtd)
{
    enumDef *ed;

    for (ed = pt->enums; ed != NULL; ed = ed->next)
        if (ed->pyname != NULL && strcmp(ed->pyname->text, name) == 0 && ed->ecd == scope_cd && ed->emtd == scope_mtd)
            return ed;

    return NULL;
}


/*
 * Lookup a mapped type.
 */
static mappedTypeDef *lookupMappedType(sipSpec *pt, const char *name)
{
    mappedTypeDef *mtd;

    for (mtd = pt->mappedtypes; mtd != NULL; mtd = mtd->next)
        if (mtd->pyname != NULL && strcmp(mtd->pyname->text, name) == 0)
        {
            mappedTypeDef *impl = getMappedTypeImplementation(pt, mtd);

            if (impl != NULL)
                return impl;
        }

    return NULL;
}


/*
 * Lookup a class.
 */
static classDef *lookupClass(sipSpec *pt, const char *name, classDef *scope_cd)
{
    classDef *cd;

    for (cd = pt->classes; cd != NULL; cd = cd->next)
        if (strcmp(cd->pyname->text, name) == 0 && cd->ecd == scope_cd)
        {
            classDef *impl = getClassImplementation(pt, cd);

            if (impl != NULL)
                return impl;
        }

    return NULL;
}


/*
 * Get the implementation (if there is one) for a type for the default API
 * version.
 */
void getDefaultImplementation(sipSpec *pt, argType atype, classDef **cdp,
        mappedTypeDef **mtdp)
{
    classDef *cd;
    mappedTypeDef *mtd;
    ifaceFileDef *iff;

    if (atype == class_type)
    {
        cd = *cdp;
        mtd = NULL;
        iff = cd->iff;
    }
    else
    {
        cd = NULL;
        mtd = *mtdp;
        iff = mtd->iff;
    }

    /* See if there is more than one implementation. */
    if (iff->api_range != NULL)
    {
        int def_api;

        cd = NULL;
        mtd = NULL;

        /* Find the default implementation. */
        def_api = findAPI(pt, iff->api_range->api_name->text)->from;

        for (iff = iff->first_alt; iff != NULL; iff = iff->next_alt)
        {
            apiVersionRangeDef *avd = iff->api_range;

            if (avd->from > 0 && avd->from > def_api)
                continue;

            if (avd->to > 0 && avd->to <= def_api)
                continue;

            /* It's within range. */
            if (iff->type == class_iface)
            {
                for (cd = pt->classes; cd != NULL; cd = cd->next)
                    if (cd->iff == iff)
                        break;
            }
            else
            {
                for (mtd = pt->mappedtypes; mtd != NULL; mtd = mtd->next)
                    if (mtd->iff == iff)
                        break;
            }

            break;
        }
    }

    *cdp = cd;
    *mtdp = mtd;
}


/*
 * Return TRUE if a version range includes the default API.
 */
static int inDefaultAPI(sipSpec *pt, apiVersionRangeDef *range)
{
    int def_api;

    /* Handle the trivial case. */
    if (range == NULL)
        return TRUE;

    /* Get the default API. */
    def_api = findAPI(pt, range->api_name->text)->from;

    if (range->from > 0 && range->from > def_api)
        return FALSE;

    if (range->to > 0 && range->to <= def_api)
        return FALSE;

    return TRUE;
}


/*
 * Get the class implementation (if there is one) of the given class according
 * to the default version of any relevant API.
 */
static classDef *getClassImplementation(sipSpec *pt, classDef *cd)
{
    mappedTypeDef *mtd;

    getDefaultImplementation(pt, class_type, &cd, &mtd);

    return cd;
}


/*
 * Get the mapped type implementation (if there is one) of the given mapped
 * type according to the default version of any relevant API.
 */
static mappedTypeDef *getMappedTypeImplementation(sipSpec *pt,
        mappedTypeDef *mtd)
{
    classDef *cd;

    getDefaultImplementation(pt, mapped_type, &cd, &mtd);

    return mtd;
}
