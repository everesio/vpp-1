#!/usr/bin/env python2
#
# Copyright (c) 2018 Cisco and/or its affiliates.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
from string import Template

from jni_common_gen import generate_j2c_swap, generate_j2c_identifiers, generate_c2j_swap
from jvpp_model import Class, Enum


def generate_type_handlers(model, logger):
    """
    Generates host-to-net and net-to-host functions for all custom types defined in the VPP API
    :param model: meta-model of VPP API used for jVPP generation.
    :param logger: jVPP logger
    """
    type_handlers = []
    for t in model.types:
        #TODO(VPP-1186): move the logic to JNI generators
        if isinstance(t, Class):
            _generate_class(model, t, type_handlers)
        elif isinstance(t, Enum):
            _generate_enum(model, t, type_handlers)
        else:
            logger.debug("Skipping custom JNI type handler generation for %s", t)

    return "\n".join(type_handlers)


def _generate_class(model, t, type_handlers):
    ref_name = t.java_name_lower
    jni_identifiers = generate_j2c_identifiers(t, class_ref_name="%sClass" % ref_name, object_ref_name="_host")
    type_handlers.append(_TYPE_NET_TO_HOST_TEMPLATE.substitute(
        c_name=t.name,
        json_filename=model.json_api_files,
        json_definition=t.doc,
        type_reference_name=ref_name,
        class_FQN=t.jni_name,
        jni_identifiers=jni_identifiers,
        type_swap=generate_j2c_swap(t, struct_ref_name="_net")
    ))

    type_handlers.append(_TYPE_HOST_TO_NET_TEMPLATE.substitute(
        c_name=t.name,
        json_filename=model.json_api_files,
        json_definition=t.doc,
        type_reference_name=ref_name,
        class_FQN=t.jni_name,
        jni_identifiers=jni_identifiers,
        type_swap=generate_c2j_swap(t, object_ref_name="_host", struct_ref_name="_net")
    ))

_TYPE_NET_TO_HOST_TEMPLATE = Template("""
/**
 * Host to network byte order conversion for ${c_name} type.
 * Generated based on $json_filename:
$json_definition
 */
static inline void _host_to_net_${c_name}(JNIEnv * env, jobject _host, vl_api_${c_name}_t * _net)
{
    jclass ${type_reference_name}Class = (*env)->FindClass(env, "${class_FQN}");
$jni_identifiers
$type_swap
}""")

_TYPE_HOST_TO_NET_TEMPLATE = Template("""
/**
 * Network to host byte order conversion for ${c_name} type.
 * Generated based on $json_filename:
$json_definition
 */
static inline void _net_to_host_${c_name}(JNIEnv * env, vl_api_${c_name}_t * _net, jobject _host)
{
    jclass ${type_reference_name}Class = (*env)->FindClass(env, "${class_FQN}");
$type_swap
}""")


def _generate_enum(model, t, type_handlers):
    value_type = t.value.type
    type_handlers.append(_ENUM_NET_TO_HOST_TEMPLATE.substitute(
        c_name=t.name,
        json_filename=model.json_api_files,
        json_definition=t.doc,
        class_FQN=t.jni_name,
        jni_signature=value_type.jni_signature,
        jni_type=value_type.jni_type,
        jni_accessor=value_type.jni_accessor,
        swap=_generate_scalar_host_to_net_swap(t.value)
    ))

    type_handlers.append(_ENUM_HOST_TO_NET_TEMPLATE.substitute(
        c_name=t.name,
        json_filename=model.json_api_files,
        json_definition=t.doc,
        class_FQN=t.jni_name,
        jni_type=value_type.jni_type,
        type_swap=_generate_scalar_net_to_host_swap(t.value)
    ))

_ENUM_NET_TO_HOST_TEMPLATE = Template("""
/**
 * Host to network byte order conversion for ${c_name} enum.
 * Generated based on $json_filename:
$json_definition
 */
static inline void _host_to_net_${c_name}(JNIEnv * env, jobject _host, vl_api_${c_name}_t * _net)
{
    jclass enumClass = (*env)->FindClass(env, "${class_FQN}");
    jfieldID valueFieldId = (*env)->GetStaticFieldID(env, enumClass, "value", "${jni_signature}");
    ${jni_type} value = (*env)->GetStatic${jni_accessor}Field(env, enumClass, valueFieldId);
    ${swap};
}""")

_ENUM_HOST_TO_NET_TEMPLATE = Template("""
/**
 * Network to host byte order conversion for ${c_name} type.
 * Generated based on $json_filename:
$json_definition
 */
static inline ${jni_type} _net_to_host_${c_name}(vl_api_${c_name}_t _net)
{
    return (${jni_type}) $type_swap
}""")


def _generate_scalar_host_to_net_swap(field):
    field_type = field.type
    if field_type.is_swap_needed:
        return field_type.get_host_to_net_function(field.java_name, "*_net")
    else:
        return "*_net = %s" % field.java_name


def _generate_scalar_net_to_host_swap(field):
    field_type = field.type
    if field_type.is_swap_needed:
        return "%s((%s) _net);" % (field_type.net_to_host_function, field_type.name)
    else:
        return "_net"