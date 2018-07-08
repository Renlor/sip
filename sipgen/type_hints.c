/*
 * The PEP 484 type hints generator for SIP.
 *
 * Copyright (c) 2017 Riverbank Computing Limited <info@riverbankcomputing.com>
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


/* Return a string referring to an object of any type. */
#define anyObject(pep484)   ((pep484) ? "typing.Any" : "object")


static void pyiCompositeModule(sipSpec *pt, moduleDef *comp_mod, FILE *fp);
static void pyiModule(sipSpec *pt, moduleDef *mod, FILE *fp);
static void pyiTypeHintCode(codeBlockList *thc, int indent, FILE *fp);
static void pyiEnums(sipSpec *pt, moduleDef *mod, ifaceFileDef *scope,
        ifaceFileList *defined, int indent, FILE *fp);
static void pyiVars(sipSpec *pt, moduleDef *mod, classDef *scope,
        ifaceFileList *defined, int indent, FILE *fp);
static void pyiClass(sipSpec *pt, moduleDef *mod, classDef *cd,
        ifaceFileList **defined, int indent, FILE *fp);
static void pyiMappedType(sipSpec *pt, moduleDef *mod, mappedTypeDef *mtd,
        ifaceFileList **defined, int indent, FILE *fp);
static void pyiCtor(sipSpec *pt, moduleDef *mod, classDef *cd, ctorDef *ct,
        int overloaded, int sec, ifaceFileList *defined, int indent, FILE *fp);
static void pyiCallable(sipSpec *pt, moduleDef *mod, memberDef *md,
        overDef *overloads, int is_method, ifaceFileList *defined, int indent,
        FILE *fp);
static void pyiProperty(sipSpec *pt, moduleDef *mod, propertyDef *pd,
        int is_setter, memberDef *md, overDef *overloads,
        ifaceFileList *defined, int indent, FILE *fp);
static void pyiOverload(sipSpec *pt, moduleDef *mod, overDef *od,
        int overloaded, int is_method, int sec, ifaceFileList *defined,
        int indent, int pep484, FILE *fp);
static void pyiPythonSignature(sipSpec *pt, moduleDef *mod, signatureDef *sd,
        int need_self, int sec, ifaceFileList *defined, KwArgs kwargs,
        int pep484, FILE *fp);
static int pyiArgument(sipSpec *pt, moduleDef *mod, argDef *ad, int arg_nr,
        int out, int need_comma, int sec, int names, int defaults,
        ifaceFileList *defined, KwArgs kwargs, int pep484, FILE *fp);
static void pyiType(sipSpec *pt, moduleDef *mod, argDef *ad, int out, int sec,
        ifaceFileList *defined, int pep484, FILE *fp);
static void pyiTypeHint(sipSpec *pt, typeHintDef *thd, moduleDef *mod, int out,
        ifaceFileList *defined, int pep484, FILE *fp);
static void pyiTypeHintNode(typeHintNodeDef *node, moduleDef *mod,
        ifaceFileList *defined, int pep484, FILE *fp);
static void prIndent(int indent, FILE *fp);
static int separate(int first, int indent, FILE *fp);
static void prClassRef(classDef *cd, moduleDef *mod, ifaceFileList *defined,
        int pep484, FILE *fp);
static void prEnumRef(enumDef *ed, moduleDef *mod, ifaceFileList *defined,
        int pep484, FILE *fp);
static void prScopedEnumName(FILE *fp, enumDef *ed);
static int isDefined(ifaceFileDef *iff, classDef *cd, moduleDef *mod,
        ifaceFileList *defined);
static int inIfaceFileList(ifaceFileDef *iff, ifaceFileList *defined);
static void parseTypeHint(sipSpec *pt, typeHintDef *thd, int out);
static int parseTypeHintNode(sipSpec *pt, int out, int top_level, char *start,
        char *end, typeHintNodeDef **thnp);
static const char *typingModule(const char *name);
static typeHintNodeDef *lookupType(sipSpec *pt, char *name, int out);
static enumDef *lookupEnum(sipSpec *pt, const char *name, classDef *scope_cd,
        mappedTypeDef *scope_mtd);
static mappedTypeDef *lookupMappedType(sipSpec *pt, const char *name);
static classDef *lookupClass(sipSpec *pt, const char *name,
        classDef *scope_cd);
static classDef *getClassImplementation(sipSpec *pt, classDef *cd);
static mappedTypeDef *getMappedTypeImplementation(sipSpec *pt,
        mappedTypeDef *mtd);
static void maybeAnyObject(const char *hint, int pep484, FILE *fp);
static void strip_leading(char **startp, char *end);
static void strip_trailing(char *start, char **endp);
static typeHintNodeDef *flatten_unions(typeHintNodeDef *nodes);
static typeHintNodeDef *copyTypeHintNode(sipSpec *pt, typeHintDef *thd,
        int out);


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
"import typing\n"
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

    /*
     * Generate any exported type hint code and any module-specific type hint
     * code.
     */
    pyiTypeHintCode(pt->exptypehintcode, 0, fp);
    pyiTypeHintCode(mod->typehintcode, 0, fp);

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

        impl = getClassImplementation(pt, cd);

        if (impl != NULL)
        {
            if (impl->no_typehint)
                continue;

            /* Only handle non-nested classes here. */
            if (impl->ecd != NULL)
                continue;

            pyiClass(pt, mod, impl, &defined, 0, fp);
        }
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
static void pyiTypeHintCode(codeBlockList *thc, int indent, FILE *fp)
{
    while (thc != NULL)
    {
        int need_indent = TRUE;
        const char *cp;

        fprintf(fp, "\n");

        for (cp = thc->block->frag; *cp != '\0'; ++cp)
        {
            if (need_indent)
            {
                need_indent = FALSE;
                prIndent(indent, fp);
            }

            fprintf(fp, "%c", *cp);

            if (*cp == '\n')
                need_indent = TRUE;
        }

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
    propertyDef *pd;

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

            prClassRef(cl->cd, mod, *defined, TRUE, fp);
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

    no_body = (cd->typehintcode == NULL && nr_overloads == 0);

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

    pyiTypeHintCode(cd->typehintcode, indent, fp);

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

        pyiCtor(pt, mod, NULL, ct, overloaded, FALSE, *defined, indent, fp);

        if (implicit_overloads)
            pyiCtor(pt, mod, NULL, ct, overloaded, TRUE, *defined, indent, fp);
    }

    first = TRUE;

    for (md = cd->members; md != NULL; md = md->next)
    {
        /*
         * Ignore slots which can return Py_NotImplemented as code may be
         * correctly handled elsewhere.  We also have to include the sequence
         * slots because they can't be distinguished from the number slots of
         * the same name.
         */
        if (isNumberSlot(md) || isInplaceNumberSlot(md) || isRichCompareSlot(md) || md->slot == concat_slot || md->slot == iconcat_slot || md->slot == repeat_slot || md->slot == irepeat_slot)
            continue;

        first = separate(first, indent, fp);

        pyiCallable(pt, mod, md, cd->overs, TRUE, *defined, indent, fp);
    }

    for (pd = cd->properties; pd != NULL; pd = pd->next)
    {
        first = separate(first, indent, fp);

        if (pd->get != NULL)
        {
            if ((md = findMethod(cd, pd->get)) != NULL)
            {
                pyiProperty(pt, mod, pd, FALSE, md, cd->overs, *defined,
                        indent, fp);

                if (pd->set != NULL)
                {
                    if ((md = findMethod(cd, pd->set)) != NULL)
                        pyiProperty(pt, mod, pd, TRUE, md, cd->overs, *defined,
                                indent, fp);
                }
            }
        }
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
 * Generate a ctor docstring.
 */
void dsCtor(sipSpec *pt, classDef *cd, ctorDef *ct, int sec, FILE *fp)
{
    pyiCtor(pt, pt->module, cd, ct, FALSE, sec, NULL, 0, fp);
}


/*
 * Generate an ctor type hint.
 */
static void pyiCtor(sipSpec *pt, moduleDef *mod, classDef *cd, ctorDef *ct,
        int overloaded, int sec, ifaceFileList *defined, int indent, FILE *fp)
{
    int a, need_comma;

    if (overloaded)
    {
        prIndent(indent, fp);
        fprintf(fp, "@typing.overload\n");
    }

    prIndent(indent, fp);

    if (cd == NULL)
    {
        fprintf(fp, "def __init__(self");
        need_comma = TRUE;
    }
    else
    {
        prScopedPythonName(fp, cd->ecd, cd->pyname->text);
        fprintf(fp, "(");
        need_comma = FALSE;
    }

    for (a = 0; a < ct->pysig.nrArgs; ++a)
        need_comma = pyiArgument(pt, mod, &ct->pysig.args[a], a, FALSE,
                need_comma, sec, TRUE, TRUE, defined, ct->kwargs, (cd == NULL),
                fp);

    fprintf(fp, (cd == NULL) ? ") -> None: ...\n" : ")");
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
                prEnumRef(ed, mod, defined, TRUE, fp);
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
        pyiType(pt, mod, &vd->type, FALSE, FALSE, defined, TRUE, fp);
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
                TRUE, fp);

        if (implicit_overloads)
            pyiOverload(pt, mod, od, overloaded, is_method, TRUE, defined,
                    indent, TRUE, fp);
    }
}


/*
 * Generate the type hints for a property.
 */
static void pyiProperty(sipSpec *pt, moduleDef *mod, propertyDef *pd,
        int is_setter, memberDef *md, overDef *overloads,
        ifaceFileList *defined, int indent, FILE *fp)
{
    overDef *od;

    /* Handle each overload. */
    for (od = overloads; od != NULL; od = od->next)
    {
        if (isPrivate(od))
            continue;

        if (od->common != md)
            continue;

        if (od->no_typehint)
            continue;

        prIndent(indent, fp);

        if (is_setter)
            fprintf(fp, "@%s.setter\n", pd->name->text);
        else
            fprintf(fp, "@property\n");

        prIndent(indent, fp);

        fprintf(fp, "def %s", pd->name->text);

        pyiPythonSignature(pt, mod, &od->pysig, TRUE, FALSE, defined,
                od->kwargs, TRUE, fp);

        fprintf(fp, ": ...\n");

        break;
    }
}


/*
 * Generate the docstring for a single API overload.
 */
void dsOverload(sipSpec *pt, overDef *od, int is_method, int sec, FILE *fp)
{
    pyiOverload(pt, pt->module, od, FALSE, is_method, sec, NULL, 0, FALSE, fp);
}


/*
 * Generate the type hints for a single API overload.
 */
static void pyiOverload(sipSpec *pt, moduleDef *mod, overDef *od,
        int overloaded, int is_method, int sec, ifaceFileList *defined,
        int indent, int pep484, FILE *fp)
{
    int need_self;

    if (overloaded)
    {
        prIndent(indent, fp);
        fprintf(fp, "@typing.overload\n");
    }

    if (pep484 && is_method && isStatic(od))
    {
        prIndent(indent, fp);
        fprintf(fp, "@staticmethod\n");
    }

    prIndent(indent, fp);
    fprintf(fp, "%s%s", (pep484 ? "def " : ""), od->common->pyname->text);

    need_self = (is_method && !isStatic(od));

    pyiPythonSignature(pt, mod, &od->pysig, need_self, sec, defined,
            od->kwargs, pep484, fp);

    if (pep484)
        fprintf(fp, ": ...\n");
}


/*
 * Generate a Python argument.
 */
static int pyiArgument(sipSpec *pt, moduleDef *mod, argDef *ad, int arg_nr,
        int out, int need_comma, int sec, int names, int defaults,
        ifaceFileList *defined, KwArgs kwargs, int pep484, FILE *fp)
{
    int optional, use_optional;

    if (isArraySize(ad))
        return need_comma;

    if (sec && (ad->atype == slotcon_type || ad->atype == slotdis_type))
        return need_comma;

    if (need_comma)
        fprintf(fp, ", ");

    optional = (defaults && ad->defval && !out);

    /*
     * We only show names for PEP 484 type hints and when they are part of the
     * API.
     */
    if (names)
        names = (pep484 || kwargs == AllKwArgs || (kwargs == OptionalKwArgs && optional));

    if (names && ad->atype != ellipsis_type)
    {
        if (ad->name != NULL)
            fprintf(fp, "%s%s: ", ad->name->text,
                    (isPyKeyword(ad->name->text) ? "_" : ""));
        else
            fprintf(fp, "a%d: ", arg_nr);
    }

    use_optional = FALSE;

    if (optional && pep484)
    {
        /* Assume pointers can be None unless specified otherwise. */
        if (isAllowNone(ad) || (!isDisallowNone(ad) && ad->nrderefs > 0))
        {
            fprintf(fp, "typing.Optional[");
            use_optional = TRUE;
        }
    }

    pyiType(pt, mod, ad, out, sec, defined, pep484, fp);

    if (names && ad->atype == ellipsis_type)
    {
        if (ad->name != NULL)
            fprintf(fp, "%s%s", ad->name->text,
                    (isPyKeyword(ad->name->text) ? "_" : ""));
        else
            fprintf(fp, "a%d", arg_nr);
    }

    if (optional)
    {
        if (use_optional)
            fprintf(fp, "]");

        fprintf(fp, " = ");

        if (pep484)
            fprintf(fp, "...");
        else
            prDefaultValue(ad, TRUE, fp);
    }

    return TRUE;
}


/*
 * Generate the default value of an argument.
 */
void prDefaultValue(argDef *ad, int in_str, FILE *fp)
{
    /* Use any explicitly provided documentation. */
    if (ad->typehint_value != NULL)
    {
        fprintf(fp, "%s", ad->typehint_value);
        return;
    }

    /* Translate some special cases. */
    if (ad->defval->next == NULL && ad->defval->vtype == numeric_value)
    {
        if (ad->nrderefs > 0 && ad->defval->u.vnum == 0)
        {
            fprintf(fp, "None");
            return;
        }

        if (ad->atype == bool_type || ad->atype == cbool_type)
        {
            fprintf(fp, ad->defval->u.vnum ? "True" : "False");
            return;
        }
    }

    /* SIP v5 won't need this. */
    prcode(fp, "%M");
    generateExpression(ad->defval, in_str, fp);
    prcode(fp, "%M");
}


/*
 * Generate the Python representation of a type.
 */
static void pyiType(sipSpec *pt, moduleDef *mod, argDef *ad, int out, int sec,
        ifaceFileList *defined, int pep484, FILE *fp)
{
    const char *type_name;
    typeHintDef *thd;

    /* Use any explicit type hint unless the argument is constrained. */
    thd = (out ? ad->typehint_out : (isConstrained(ad) ? NULL : ad->typehint_in));

    if (thd != NULL)
    {
        pyiTypeHint(pt, thd, mod, out, defined, pep484, fp);
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
            prClassRef(cd, mod, defined, pep484, fp);
        }
        else
        {
            /*
             * This should never happen as it should have been picked up when
             * generating code - but maybe we haven't been asked to generate
             * code.
             */
            fprintf(fp, anyObject(pep484));
        }

        return;
    }

    type_name = NULL;

    switch (ad->atype)
    {
    case enum_type:
        if (ad->u.ed->pyname != NULL)
            prEnumRef(ad->u.ed, mod, defined, pep484, fp);
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
        type_name = "QT_SIGNAL";
        break;

    case slot_type:
        type_name = "QT_SLOT_QT_SIGNAL";
        break;

    case rxcon_type:
    case rxdis_type:
        if (sec)
        {
            type_name = (pep484 ? "typing.Callable[..., None]" : "Callable[..., None]");
        }
        else
        {
            /* The class should always be found. */
            if (pt->qobject_cd != NULL)
                prClassRef(pt->qobject_cd, mod, defined, pep484, fp);
            else
                type_name = anyObject(pep484);
        }

        break;

    case qobject_type:
        type_name = "QObject";
        break;

    case ustring_type:
        /* Correct for Python v3. */
        type_name = "bytes";
        break;

    case string_type:
    case sstring_type:
    case wstring_type:
    case ascii_string_type:
    case latin1_string_type:
    case utf8_string_type:
        type_name = isArray(ad) ? "bytes" : "str";
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
        type_name = anyObject(pep484);
        break;

    case pytuple_type:
        type_name = (pep484 ? "typing.Tuple" : "Tuple");
        break;

    case pylist_type:
        type_name = (pep484 ? "typing.List" : "List");
        break;

    case pydict_type:
        type_name = (pep484 ? "typing.Dict" : "Dict");
        break;

    case pycallable_type:
        type_name = (pep484 ? "typing.Callable[..., None]" : "Callable[..., None]");
        break;

    case pyslice_type:
        type_name = "slice";
        break;

    case pytype_type:
        type_name = "type";
        break;

    case pybuffer_type:
        type_name = "sip.Buffer";
        break;

    case ellipsis_type:
        type_name = "*";
        break;

    case slotcon_type:
    case anyslot_type:
        type_name = "QT_SLOT";
        break;

    default:
        type_name = anyObject(pep484);
    }

    if (type_name != NULL)
        fprintf(fp, "%s", type_name);
}


/*
 * Generate a scoped Python name.
 */
void prScopedPythonName(FILE *fp, classDef *scope, const char *pyname)
{
    if (scope != NULL && !isHiddenNamespace(scope))
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
        int need_self, int sec, ifaceFileList *defined, KwArgs kwargs,
        int pep484, FILE *fp)
{
    int void_return, need_comma, is_res, nr_out, a;

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
                TRUE, defined, kwargs, pep484, fp);
    }

    fprintf(fp, ")");

    /* An empty type hint specifies a void return. */
    if (sd->result.typehint_out != NULL)
        void_return = (sd->result.typehint_out->raw_hint[0] == '\0');
    else
        void_return = FALSE;

    is_res = !((sd->result.atype == void_type && sd->result.nrderefs == 0) ||
            void_return);

    if (is_res || nr_out > 0)
    {
        fprintf(fp, " -> ");

        if ((is_res && nr_out > 0) || nr_out > 1)
            fprintf(fp, "%sTuple[", (pep484 ? "typing." : ""));

        if (is_res)
            need_comma = pyiArgument(pt, mod, &sd->result, -1, TRUE, FALSE,
                    sec, FALSE, FALSE, defined, kwargs, pep484, fp);
        else
            need_comma = FALSE;

        for (a = 0; a < sd->nrArgs; ++a)
        {
            argDef *ad = &sd->args[a];

            if (isOutArg(ad))
                /* We don't want the name in the result tuple. */
                need_comma = pyiArgument(pt, mod, ad, -1, TRUE, need_comma,
                        sec, FALSE, FALSE, defined, kwargs, pep484, fp);
        }

        if ((is_res && nr_out > 0) || nr_out > 1)
            fprintf(fp, "]");
    }
    else if (pep484)
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
static void prClassRef(classDef *cd, moduleDef *mod, ifaceFileList *defined,
        int pep484, FILE *fp)
{
    if (pep484)
    {
        /*
         * We assume that an external class will be handled properly by some
         * handwritten type hint code.
         */
        int is_defined = (isExternal(cd) || isDefined(cd->iff, cd->ecd, mod, defined));

        if (!is_defined)
            fprintf(fp, "'");

        if (cd->iff->module != mod)
            fprintf(fp, "%s.", cd->iff->module->name);

        prScopedPythonName(fp, cd->ecd, cd->pyname->text);

        if (!is_defined)
            fprintf(fp, "'");
    }
    else
    {
        prScopedPythonName(fp, cd->ecd, cd->pyname->text);
    }
}


/*
 * Generate an enum reference, including its owning module if necessary and
 * handling forward references if necessary.
 */
static void prEnumRef(enumDef *ed, moduleDef *mod, ifaceFileList *defined,
        int pep484, FILE *fp)
{
    if (pep484)
    {
        int is_defined;

        if (ed->ecd != NULL)
        {
            is_defined = isDefined(ed->ecd->iff, ed->ecd->ecd, mod, defined);
        }
        else if (ed->emtd != NULL)
        {
            is_defined = isDefined(ed->emtd->iff, NULL, mod, defined);
        }
        else
        {
            /* Global enums are defined early on. */
            is_defined = TRUE;
        }

        if (!is_defined)
            fprintf(fp, "'");

        if (ed->module != mod)
            fprintf(fp, "%s.", ed->module->name);

        prScopedEnumName(fp, ed);

        if (!is_defined)
            fprintf(fp, "'");
    }
    else
    {
        prScopedEnumName(fp, ed);
    }
}


/*
 * Generate a scoped enum name.
 */
static void prScopedEnumName(FILE *fp, enumDef *ed)
{
    if (ed->emtd != NULL)
        fprintf(fp, "%s.%s", ed->emtd->pyname->text, ed->pyname->text);
    else
        prScopedPythonName(fp, ed->ecd, ed->pyname->text);
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
int hasImplicitOverloads(signatureDef *sd)
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

    thd->status = needs_parsing;
    thd->raw_hint = raw_hint;

    return thd;
}


/*
 * Generate a type hint from a /TypeHint/ annotation.
 */
static void pyiTypeHint(sipSpec *pt, typeHintDef *thd, moduleDef *mod, int out,
        ifaceFileList *defined, int pep484, FILE *fp)
{
    parseTypeHint(pt, thd, out);

    if (thd->root != NULL)
        pyiTypeHintNode(thd->root, mod, defined, pep484, fp);
    else
        maybeAnyObject(thd->raw_hint, pep484, fp);
}


/*
 * Generate a single node of a type hint.
 */
static void pyiTypeHintNode(typeHintNodeDef *node, moduleDef *mod,
        ifaceFileList *defined, int pep484, FILE *fp)
{
    switch (node->type)
    {
    case typing_node:
        fprintf(fp, "%s%s", (pep484 ? "typing." : ""), node->u.name);

        if (node->children != NULL)
        {
            int need_comma = FALSE;
            typeHintNodeDef *thnd;

            fprintf(fp, "[");

            for (thnd = node->children; thnd != NULL; thnd = thnd->next)
            {
                if (need_comma)
                    fprintf(fp, ", ");

                need_comma = TRUE;

                pyiTypeHintNode(thnd, mod, defined, pep484, fp);
            }

            fprintf(fp, "]");
        }

        break;

    case class_node:
        prClassRef(node->u.cd, mod, defined, pep484, fp);
        break;

    case enum_node:
        prEnumRef(node->u.ed, mod, defined, pep484, fp);
        break;

    case brackets_node:
        fprintf(fp, "[]");
        break;

    case other_node:
        maybeAnyObject(node->u.name, pep484, fp);
        break;
    }
}


/*
 * Parse a type hint and update its status accordingly.
 */
static void parseTypeHint(sipSpec *pt, typeHintDef *thd, int out)
{
    if (thd->status == needs_parsing)
    {
        thd->status = being_parsed;
        parseTypeHintNode(pt, out, TRUE, thd->raw_hint,
                thd->raw_hint + strlen(thd->raw_hint), &thd->root);
        thd->status = parsed;
    }
}


/*
 * Recursively parse a type hint.  Return FALSE if the parse failed.
 */
static int parseTypeHintNode(sipSpec *pt, int out, int top_level, char *start,
        char *end, typeHintNodeDef **thnp)
{
    char *cp, *name_start, *name_end;
    int have_brackets = FALSE;
    typeHintNodeDef *node, *children = NULL;

    /* Assume there won't be a node. */
    *thnp = NULL;

    /* Find the name and any opening and closing bracket. */
    strip_leading(&start, end);
    name_start = start;

    strip_trailing(start, &end);
    name_end = end;

    for (cp = start; cp < end; ++cp)
        if (*cp == '[')
        {
            typeHintNodeDef **tail = &children;

            /* The last character must be a closing bracket. */
            if (end[-1] != ']')
                return FALSE;

            /* Find the end of any name. */
            name_end = cp;
            strip_trailing(name_start, &name_end);

            for (;;)
            {
                char *pp;
                int depth;

                /* Skip the opening bracket or comma. */
                ++cp;

                /* Find the next comma, if any. */
                depth = 0;

                for (pp = cp; pp < end; ++pp)
                    if (*pp == '[')
                    {
                        ++depth;
                    }
                    else if (*pp == ']' && depth != 0)
                    {
                        --depth;
                    }
                    else if ((*pp == ',' || *pp == ']') && depth == 0)
                    {
                        typeHintNodeDef *child;

                        /* Recursively parse this part. */
                        if (!parseTypeHintNode(pt, out, FALSE, cp, pp, &child))
                            return FALSE;

                        if (child != NULL)
                        {
                            /*
                             * Append the child to the list of children.  There
                             * might not be a child if we have detected a
                             * recursive definition.
                             */
                            *tail = child;
                            tail = &child->next;
                        }

                        cp = pp;
                        break;
                    }

                if (pp == end)
                    break;
            }

            have_brackets = TRUE;

            break;
        }

    /* We must have a name unless we have empty brackets. */
    if (name_start == name_end)
    {
        if (top_level && have_brackets && children == NULL)
            return FALSE;

        /* Return the representation of empty brackets. */
        node = sipMalloc(sizeof (typeHintNodeDef));
        node->type = brackets_node;
    }
    else
    {
        char saved;
        const char *typing;

        /* Isolate the name. */
        saved = *name_end;
        *name_end = '\0';

        /* See if it is an object in the typing module. */
        if ((typing = typingModule(name_start)) != NULL)
        {
            if (strcmp(typing, "Union") == 0)
            {
                /*
                 * If there are no children assume it is because they have been
                 * omitted.
                 */
                if (children == NULL)
                    return TRUE;

                children = flatten_unions(children);
            }

            node = sipMalloc(sizeof (typeHintNodeDef));
            node->type = typing_node;
            node->u.name = typing;
            node->children = children;
        }
        else
        {
            /* Search for the type. */
            node = lookupType(pt, name_start, out);
        }

        *name_end = saved;

        /* Only objects from the typing module can have brackets. */
        if (typing == NULL && have_brackets)
            return FALSE;
    }

    *thnp = node;

    return TRUE;
}


/*
 * Strip leading spaces from a string.
 */
static void strip_leading(char **startp, char *end)
{
    char *start;

    start = *startp;

    while (start < end && start[0] == ' ')
        ++start;

    *startp = start;
}


/*
 * Strip trailing spaces from a string.
 */
static void strip_trailing(char *start, char **endp)
{
    char *end;

    end = *endp;

    while (end > start && end[-1] == ' ')
        --end;

    *endp = end;
}


/*
 * Look up an object in the typing module.
 */
static const char *typingModule(const char *name)
{
    static const char *typing[] = {
        "Any",
        "Callable",
        "Dict",
        "Iterable",
        "Iterator",
        "List",
        "Mapping",
        "NamedTuple",
        "Optional",
        "Sequence",
        "Set",
        "Tuple",
        "Union",
        NULL
    };

    const char **np;

    for (np = typing; *np != NULL; ++np)
        if (strcmp(*np, name) == 0)
            return *np;

    return NULL;
}


/*
 * Flatten an unions in a list of nodes.
 */
static typeHintNodeDef *flatten_unions(typeHintNodeDef *nodes)
{
    typeHintNodeDef *head, **tailp, *thnd;

    head = NULL;
    tailp = &head;

    for (thnd = nodes; thnd != NULL; thnd = thnd->next)
    {
        if (thnd->type == typing_node && strcmp(thnd->u.name, "Union") == 0)
        {
            typeHintNodeDef *child;

            for (child = thnd->children; child != NULL; child = child->next)
            {
                *tailp = child;
                tailp = &child->next;
            }
        }
        else
        {
            /* Move this one to the new list. */
            *tailp = thnd;
            tailp = &thnd->next;
        }
    }

    *tailp = NULL;

    return head;
}


/*
 * Look up a qualified Python type and return the corresponding node (or NULL
 * if the type should be omitted because of a recursive definition).
 */
static typeHintNodeDef *lookupType(sipSpec *pt, char *name, int out)
{
    char *sp, *ep;
    classDef *scope_cd;
    mappedTypeDef *scope_mtd;
    typeHintDef *thd;
    typeHintNodeDef *node;

    /* Start searching at the global level. */
    scope_cd = NULL;
    scope_mtd = NULL;

    sp = name;
    ep = NULL;

    while (*sp != '\0')
    {
        enumDef *ed;

        /* Isolate the next part of the name. */
        if ((ep = strchr(sp, '.')) != NULL)
            *ep = '\0';

        /* See if it's an enum. */
        if ((ed = lookupEnum(pt, sp, scope_cd, scope_mtd)) != NULL)
        {
            /* Make sure we have used the whole name. */
            if (ep == NULL)
            {
                node = sipMalloc(sizeof (typeHintNodeDef));
                node->type = enum_node;
                node->u.ed = ed;

                return node;
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
            if ((mtd = lookupMappedType(pt, sp)) != NULL)
            {
                /*
                 * If we have used the whole name then the lookup has
                 * succeeded.
                 */
                if (ep == NULL)
                {
                    thd = (out ? mtd->typehint_out : mtd->typehint_in);

                    if (thd != NULL && thd->status != being_parsed)
                        return copyTypeHintNode(pt, thd, out);

                    /*
                     * If we get here we have a recursively defined mapped type
                     * so we simply omit it.
                     */
                    return NULL;
                }

                /* Otherwise this is the scope for the next part. */
                scope_mtd = mtd;
            }
        }

        if (scope_mtd == NULL)
        {
            classDef *cd;

            /* If we get here then it must be a class. */
            if ((cd = lookupClass(pt, sp, scope_cd)) == NULL)
                break;

            /* If we have used the whole name then the lookup has succeeded. */
            if (ep == NULL)
            {
                thd = (out ? cd->typehint_out : cd->typehint_in);

                if (thd != NULL && thd->status != being_parsed)
                    return copyTypeHintNode(pt, thd, out);

                node = sipMalloc(sizeof (typeHintNodeDef));
                node->type = class_node;
                node->u.cd = cd;

                return node;
            }

            /* Otherwise this is the scope for the next part. */
            scope_cd = cd;
        }

        /* If we have run out of name then the lookup has failed. */
        if (ep == NULL)
            break;

        /* Repair the name and go on to the next part. */
        *ep++ = '.';
        sp = ep;
    }

    /* Repair the name. */
    if (ep != NULL)
        *ep = '.';

    /* Nothing was found. */
    node = sipMalloc(sizeof (typeHintNodeDef));
    node->type = other_node;
    node->u.name = sipStrdup(name);

    return node;
}


/*
 * Copy the root node of a type hint.
 */
static typeHintNodeDef *copyTypeHintNode(sipSpec *pt, typeHintDef *thd,
        int out)
{
    typeHintNodeDef *node;

    parseTypeHint(pt, thd, out);

    if (thd->root == NULL)
        return NULL;

    node = sipMalloc(sizeof (typeHintNodeDef));
    *node = *thd->root;
    node->next = NULL;

    return node;
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
        if (strcmp(cd->pyname->text, name) == 0 && cd->ecd == scope_cd && !isExternal(cd))
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
int inDefaultAPI(sipSpec *pt, apiVersionRangeDef *range)
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


/*
 * Generate a hint taking into account that it may be any sort of object.
 */
static void maybeAnyObject(const char *hint, int pep484, FILE *fp)
{
    fprintf(fp, "%s", (strcmp(hint, "Any") != 0 ? hint : anyObject(pep484)));
}
