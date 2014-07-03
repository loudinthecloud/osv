#!/usr/bin/env python
import json
import sys
import re
import glob
import argparse
import os

parser = argparse.ArgumentParser(description="""Generate C++ class for json
handling from swagger definition""")

parser.add_argument('-outdir', help='the output directory', default='autogen')
parser.add_argument('-f', help='input file', default='api-java.json')
parser.add_argument('-ns', help="""namespace when set struct will be created
under the namespace""", default='')
parser.add_argument('-jsoninc', help='relative path to the jsaon include',
                    default='json/')
parser.add_argument('-jsonns', help='set the json namespace', default='json')
parser.add_argument('-indir', help="""when set all json file in the given
directory will be parsed, do not use with -f""", default='')
parser.add_argument('-debug', help='debug level 0 -quite,1-error,2-verbose',
                    default='1', type=int)
parser.add_argument('-combined', help='set the name of the combined file',
                    default='autogen/pathautogen.ee')
config = parser.parse_args()


valid_vars = {'string': 'std::string', 'int': 'int', 'double': 'double',
             'float': 'float', 'long': 'long', 'boolean': 'bool', 'char': 'char',
             'datetime': 'json::date_time'}

current_file = ''

spacing = "    "

def fprint(f, *args):
    for arg in args:
        f.write(arg)
    f.write('\n')


def open_namespace(f, ns=config.ns):
    fprint(f, "namespace ", ns , ' {\n')


def close_namespace(f):
    fprint(f, '}')


def add_include(f, includes):
    for include in includes:
        fprint(f, '#include ', include)
    fprint(f, "")

def trace_verbose(*params):
    if config.debug > 1:
        print(''.join(params))


def trace_err(*params):
    if config.debug > 0:
        print(current_file +':' +''.join(params))


def valid_type(param):
    if param in valid_vars:
        return valid_vars[param]
    trace_err("Type [", param, "] not defined")
    return param


def type_change(param, member):
    if param == "array":
        if "items" not in member:
            trace_err("array without item declaration in ", param)
            return ""
        item = member["items"]
        if "type" in item:
            t = item["type"]
        elif "$ref" in item:
            t = item["$ref"]
        else:
            trace_err("array items with no type or ref declaration ", param)
            return ""
        return "json_list< " + valid_type(t) + " >"
    return "json_element< " + valid_type(param.lower()) + " >"



def print_ind_comment(f, ind, *params):
    fprint(f, ind, "/**")
    for s in params:
        fprint(f, ind, " * ", s)
    fprint(f, ind, " */")

def print_comment(f, *params):
    print_ind_comment(f, spacing, *params)

def print_copyrights(f):
    fprint(f, "/*")
    fprint(f, "* Copyright (C) 2014 Cloudius Systems, Ltd.")
    fprint(f, "*")
    fprint(f, "* This work is open source software, licensed under the",
           " terms of the")
    fprint(f, "* BSD license as described in the LICENSE f in the top-",
           "level directory.")
    fprint(f, "*")
    fprint(f, "*  This is an Auto-Generated-code  ")
    fprint(f, "*  Changes you do in this file will be erased on next",
           " code generation")
    fprint(f, "*/\n")


def print_h_file_headers(f, name):
    print_copyrights(f)
    fprint(f, "#ifndef __JSON_AUTO_GENERATED_" + name)
    fprint(f, "#define __JSON_AUTO_GENERATED_" + name + "\n")


def clean_param(param):
    match = re.match(r"(^[^\}]+)\s*}", param)
    if match:
        return match.group(1)
    return param


def get_parameter_by_name(obj, name):
    for p in obj["parameters"]:
        if p["name"] == name:
            return p
    trace_err ("No Parameter declaration found for ", name)


def clear_path_ending(path):
    if not path or path[-1] != '/':
        return path
    return path[0:-1]


def add_path(f, path, details):
    if "summary" in details:
        print_comment(f, details["summary"])

    if "{" in path:
        vals = path.split("{")
        vals.reverse()
        fprint(f, spacing, 'path_description::add_path("', clear_path_ending(vals.pop()),
           '",', details["method"], ',"', details["nickname"], '")')
        while vals:
            param = clean_param(vals.pop())
            param_type = get_parameter_by_name(details, param)
            if ("allowMultiple" in param_type and
                param_type["allowMultiple"] == True):
                fprint(f, spacing, '  ->pushparam("', param, '",true)')
            else:
                fprint(f, spacing, '  ->pushparam("', param, '")')
    else:
        fprint(f, spacing, 'path_description::add_path("', clear_path_ending(path), '",',
           details["method"], ',"', details["nickname"], '")')
    if "parameters" in details:
        for param in details["parameters"]:
            if "required" in param and param["required"] and  param["paramType"] == "query":
                fprint(f, spacing, '  ->pushmandatory_param("', param["name"], '")')
    fprint(f, spacing, ";")


def get_base_name(param):
    return os.path.basename(param)

def create_c_file(data, cfile_name, hfile_name, init_method, api_name):
    cfile = open(cfile_name, "w")
    print_copyrights(cfile)
    add_include(cfile, ['"' + hfile_name + '"' , '"' + config.jsoninc +
                        'json_path.hh"'])
    open_namespace(cfile, "httpserver")
    open_namespace(cfile)
    fprint(cfile, init_method + "()\n{")

    for item in data["apis"]:
        if "operations" in item:
            trace_verbose("path: " + item["path"])
            for oper in item["operations"]:
                add_path(cfile, item["path"], oper)
    fprint(cfile, "}")
    for item in data["apis"]:
        if "operations" in item:
            for oper in item["operations"]:
                if "parameters" in oper:
                    for param in oper["parameters"]:
                        if "enum" in param:
                            enm = 'ns_' + oper["nickname"] + '::' + param["name"]
                            fprint(cfile, api_name , '::', enm, ' ',api_name , '::ns_', oper["nickname"], '::str2', param["name"], '(const std::string& str)\n{')
                            fprint(cfile, '  static const std::string arr[] = {"', '","'.join(param["enum"]) , '"};')
                            fprint(cfile, '  int i;')
                            fprint(cfile, '  for (i=0; i < ', str(len(param["enum"])), '; i++)')
                            fprint(cfile, '    if (arr[i] == str) {return (', api_name , '::', enm , ')i;}')
                            fprint(cfile, '  return (', api_name , '::', enm , ')i;')
                            fprint(cfile, '}')


    close_namespace(cfile)
    close_namespace(cfile)
    cfile.close()

def is_model_valid(name, model):
    if name in valid_vars:
        return ""
    properties = model[name]["properties"]
    for var in properties:
        type = properties[var]["type"]
        if type == "array":
            type = properties[var]["items"]["type"]
        if type not in valid_vars:
            return type
    valid_vars[name] = name
    return ""

def resolve_model_order(data):
    res = []
    models = set()
    for model_name in data:
        visited = set(model_name)
        missing = is_model_valid(model_name, data)
        resolved = missing == ''
        if not resolved:
            stack = [model_name]
            while not resolved:
                if missing in visited:
                    trace_err ("Cyclic dependency found: ", missing)
                missing_depends = is_model_valid(missing, data)
                if missing_depends == '':
                    if missing not in models:
                        res.append(missing)
                        models.add(missing)
                    resolved = len(stack) == 0
                    if not resolved:
                        missing = stack.pop()
                else:
                    stack.append(missing)
                    missing = missing_depends
        elif model_name not in models:
            res.append(model_name)
            models.add(model_name)
    return res

def create_h_file(data, hfile_name, api_name, init_method):
    hfile = open(config.outdir + "/" + hfile_name, "w")
    print_h_file_headers(hfile, api_name)
    add_include(hfile, ['<string>', '"' + config.jsoninc +
                       'json_elements.hh"', '"path_holder.hh"'])
    open_namespace(hfile, "httpserver")
    open_namespace(hfile)
    if "models" in data:
        models_order = resolve_model_order(data["models"])
        for model_name in models_order:
            model = data["models"][model_name]
            if 'description' in model:
                print_ind_comment(hfile, "", model["description"])
            fprint(hfile, "struct ", model_name, " : public json::json_base {")
            member_init = ''
            member_assignment = ''
            member_copy = ''
            for member_name in model["properties"]:
                member = model["properties"][member_name]
                if "description" in member:
                    print_comment(hfile, member["description"])
                fprint(hfile, "  ", config.jsonns, "::",
                   type_change(member["type"], member), " ",
                   member_name, ";\n")
                member_init += "  add(&" + member_name + ',"'
                member_init += member_name + '");\n'
                member_assignment += "  " + member_name + " = " + "e." + member_name + ";\n"
                member_copy += "  e." + member_name + " = "  + member_name + ";\n"
            fprint(hfile, "void register_params() {")
            fprint(hfile, member_init)
            fprint(hfile, '}')
            
            fprint(hfile, model_name, '() {')
            fprint(hfile, '  register_params();')
            fprint(hfile, '}')
            fprint(hfile, model_name, '(const ' + model_name + ' & e) {')
            fprint(hfile, '  register_params();')
            fprint(hfile, member_assignment)
            fprint(hfile, '}')
            fprint(hfile,"template<class T>")
            fprint(hfile,model_name,"& set(const T& e) {")
            fprint(hfile,member_assignment)
            fprint(hfile,"  return *this;")
            fprint(hfile,"}")
            fprint(hfile,model_name,"& operator=(const ", model_name,"& e) {")
            fprint(hfile,member_assignment)
            fprint(hfile,"  return *this;")
            fprint(hfile,"}")
            fprint(hfile,"template<class T>")
            fprint(hfile,model_name,"& update(T& e) {")
            fprint(hfile,member_copy)
            fprint(hfile,"  return *this;")
            fprint(hfile,"}")
            fprint(hfile, "};\n\n")

    print_ind_comment(hfile, "", "Initialize the path")
    fprint(hfile, init_method + "();")
    open_namespace(hfile, api_name)
    for item in data["apis"]:
        if "operations" in item:
            for oper in item["operations"]:
                fprint(hfile, 'static const path_holder ', oper["nickname"], '("', oper["nickname"], '");')
                if "parameters" in oper:
                    for param in oper["parameters"]:
                        if "enum" in param:
                            open_namespace(hfile, 'ns_' + oper["nickname"])
                            enm =  param["name"]
                            fprint(hfile, 'enum class ', enm , ' {')
                            for val in param["enum"]:
                                hfile.write(val)
                                hfile.write(", ")
                            fprint(hfile, 'NUM_ITEMS};')
                            fprint(hfile, enm, ' str2', enm, '(const std::string& str);')
                            close_namespace(hfile)
                            
    close_namespace(hfile)
    close_namespace(hfile)
    close_namespace(hfile)
    hfile.write("#endif //__JSON_AUTO_GENERATED_HEADERS\n")
    hfile.close()

def parse_file(param, combined):
    global current_file
    trace_verbose("parsing ", param, " file")
    json_data = open(param)
    data = json.load(json_data)
    json_data.close()
    base_file_name = get_base_name(param)
    current_file = base_file_name
    hfile_name = base_file_name + ".hh"
    api_name = base_file_name.replace('.', '_')
    init_method = "void " + api_name + "_init_path"
    trace_verbose("creating ", hfile_name)

    cfile_name = config.outdir + "/" + base_file_name + ".cc"
    if (combined):
        fprint(combined, '#include "', base_file_name, ".cc", '"')
    create_c_file(data, cfile_name, hfile_name, init_method, api_name)
    create_h_file(data, hfile_name, api_name, init_method)


if "indir" in config and config.indir != '':
    combined = open(config.combined, "w")
    for f in glob.glob(os.path.join(config.indir, "*.json")):
        parse_file(f, combined)
else:
    parse_file(config.f, None)
