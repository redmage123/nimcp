/**
 * @file nimcp_knowledge_py.c
 * @brief Python bindings for NIMCP Knowledge System
 *
 * Provides Python types for:
 * - KnowledgeSystem: Multi-domain knowledge acquisition
 * - KnowledgeItem: Knowledge representation
 * - DomainKnowledge: Domain assessment results
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>
#include "cognitive/knowledge/nimcp_knowledge.h"
#include "common/nimcp_module.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(knowledge_py)

/* ============================================================================
 * KnowledgeItem Type (read-only container for retrieved knowledge)
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    char* concept_name;
    int domain;
    char* definition;
    char* context;
    float confidence;
    uint32_t reinforcement_count;
} KnowledgeItemObject;

static void KnowledgeItem_dealloc(KnowledgeItemObject* self)
{
    if (self->concept_name) {
        nimcp_free(self->concept_name);
        self->concept_name = NULL;
    }
    if (self->definition) {
        nimcp_free(self->definition);
        self->definition = NULL;
    }
    if (self->context) {
        nimcp_free(self->context);
        self->context = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMemberDef KnowledgeItem_members[] = {
    {"concept_name", T_STRING, offsetof(KnowledgeItemObject, concept_name), READONLY,
     "Name of the concept"},
    {"domain", T_INT, offsetof(KnowledgeItemObject, domain), READONLY,
     "Knowledge domain (DOMAIN_*)"},
    {"definition", T_STRING, offsetof(KnowledgeItemObject, definition), READONLY,
     "Definition of the concept"},
    {"context", T_STRING, offsetof(KnowledgeItemObject, context), READONLY,
     "Context where concept is relevant"},
    {"confidence", T_FLOAT, offsetof(KnowledgeItemObject, confidence), READONLY,
     "Confidence level (0.0-1.0)"},
    {"reinforcement_count", T_UINT, offsetof(KnowledgeItemObject, reinforcement_count), READONLY,
     "Number of times reinforced"},
    {NULL}
};

static PyObject* KnowledgeItem_repr(KnowledgeItemObject* self)
{
    return PyUnicode_FromFormat("KnowledgeItem('%s', domain=%d, confidence=%.2f)",
                                self->concept_name ? self->concept_name : "",
                                self->domain, self->confidence);
}

PyTypeObject KnowledgeItemType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.KnowledgeItem",
    .tp_doc = PyDoc_STR(
        "A piece of knowledge retrieved from the knowledge system.\n\n"
        "Attributes:\n"
        "    concept_name (str): Name of the concept\n"
        "    domain (int): Knowledge domain (use DOMAIN_* constants)\n"
        "    definition (str): What the concept means\n"
        "    context (str): When/where/why the concept is relevant\n"
        "    confidence (float): How well understood (0.0-1.0)\n"
        "    reinforcement_count (int): Times the concept was reinforced"
    ),
    .tp_basicsize = sizeof(KnowledgeItemObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)KnowledgeItem_dealloc,
    .tp_repr = (reprfunc)KnowledgeItem_repr,
    .tp_members = KnowledgeItem_members,
};

/* Helper to create KnowledgeItem from C struct */
static PyObject* KnowledgeItem_FromC(const knowledge_item_t* item)
{
    KnowledgeItemObject* obj = PyObject_New(KnowledgeItemObject, &KnowledgeItemType);
    if (obj == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obj is NULL");

        return NULL;
    }

    obj->concept_name = strdup(item->concept_name);
    obj->domain = (int)item->domain;
    obj->definition = strdup(item->definition);
    obj->context = strdup(item->context);
    obj->confidence = item->confidence;
    obj->reinforcement_count = item->reinforcement_count;

    if (!obj->concept_name || !obj->definition || !obj->context) {
        Py_DECREF(obj);
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "KnowledgeItem_FromC: required parameter is NULL (obj->concept_name, obj->definition, obj->context)");
        return NULL;
    }

    return (PyObject*)obj;
}

/* ============================================================================
 * DomainKnowledge Type (domain assessment results)
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    int domain;
    uint32_t concepts_known;
    uint32_t estimated_total;
    float coverage_percentage;
    float avg_confidence;
    PyObject* gaps;  /* List of gap strings */
} DomainKnowledgeObject;

static void DomainKnowledge_dealloc(DomainKnowledgeObject* self)
{
    Py_XDECREF(self->gaps);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyMemberDef DomainKnowledge_members[] = {
    {"domain", T_INT, offsetof(DomainKnowledgeObject, domain), READONLY,
     "Knowledge domain"},
    {"concepts_known", T_UINT, offsetof(DomainKnowledgeObject, concepts_known), READONLY,
     "Number of concepts known"},
    {"estimated_total", T_UINT, offsetof(DomainKnowledgeObject, estimated_total), READONLY,
     "Estimated total concepts in domain"},
    {"coverage_percentage", T_FLOAT, offsetof(DomainKnowledgeObject, coverage_percentage), READONLY,
     "Percentage of domain covered"},
    {"avg_confidence", T_FLOAT, offsetof(DomainKnowledgeObject, avg_confidence), READONLY,
     "Average understanding confidence"},
    {"gaps", T_OBJECT, offsetof(DomainKnowledgeObject, gaps), READONLY,
     "List of knowledge gaps"},
    {NULL}
};

static PyObject* DomainKnowledge_repr(DomainKnowledgeObject* self)
{
    return PyUnicode_FromFormat("DomainKnowledge(domain=%d, known=%u, coverage=%.1f%%)",
                                self->domain, self->concepts_known,
                                self->coverage_percentage);
}

PyTypeObject DomainKnowledgeType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.DomainKnowledge",
    .tp_doc = PyDoc_STR("Domain assessment results from knowledge system"),
    .tp_basicsize = sizeof(DomainKnowledgeObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_dealloc = (destructor)DomainKnowledge_dealloc,
    .tp_repr = (reprfunc)DomainKnowledge_repr,
    .tp_members = DomainKnowledge_members,
};

/* Helper to create DomainKnowledge from C struct */
static PyObject* DomainKnowledge_FromC(const domain_knowledge_t* dk)
{
    DomainKnowledgeObject* obj = PyObject_New(DomainKnowledgeObject, &DomainKnowledgeType);
    if (obj == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "obj is NULL");

        return NULL;
    }

    obj->domain = (int)dk->domain;
    obj->concepts_known = dk->concepts_known;
    obj->estimated_total = dk->estimated_total;
    obj->coverage_percentage = dk->coverage_percentage;
    obj->avg_confidence = dk->avg_confidence;

    /* Create gaps list */
    obj->gaps = PyList_New((Py_ssize_t)dk->num_gaps);
    if (obj->gaps == NULL) {
        Py_DECREF(obj);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "DomainKnowledge_FromC: validation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < dk->num_gaps; i++) {
        PyObject* gap_str = PyUnicode_FromString(dk->gaps[i]);
        if (gap_str == NULL) {
            Py_DECREF(obj);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "DomainKnowledge_FromC: validation failed");
            return NULL;
        }
        PyList_SET_ITEM(obj->gaps, (Py_ssize_t)i, gap_str);
    }

    return (PyObject*)obj;
}

/* ============================================================================
 * KnowledgeSystem Type
 * ============================================================================ */

typedef struct {
    PyObject_HEAD
    knowledge_system_t system;
} KnowledgeSystemObject;

/* Forward declaration for type object (defined below methods) */
extern PyTypeObject KnowledgeSystemType;

static void KnowledgeSystem_dealloc(KnowledgeSystemObject* self)
{
    if (self->system != NULL) {
        knowledge_system_destroy(self->system);
        self->system = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* KnowledgeSystem_new(PyTypeObject* type, PyObject* args, PyObject* kwds)
{
    KnowledgeSystemObject* self = (KnowledgeSystemObject*)type->tp_alloc(type, 0);
    if (self != NULL) {
        self->system = NULL;
    }
    return (PyObject*)self;
}

static int KnowledgeSystem_init(KnowledgeSystemObject* self, PyObject* args, PyObject* kwds)
{
    static char* kwlist[] = {"learner_name", NULL};
    const char* learner_name = "python_learner";

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|s", kwlist, &learner_name)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "KnowledgeSystem_init: PyArg_ParseTupleAndKeywords is NULL");
        return -1;
    }

    self->system = knowledge_system_create(learner_name);
    if (self->system == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to create knowledge system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "KnowledgeSystem_init: validation failed");
        return -1;
    }

    return 0;
}

/**
 * @brief KnowledgeSystem.learn_from_text(text, domain) -> int
 *
 * Learn concepts from text in a specific domain.
 * Returns number of concepts learned.
 */
static PyObject* KnowledgeSystem_learn_from_text(KnowledgeSystemObject* self, PyObject* args)
{
    const char* text;
    int domain = KNOWLEDGE_DOMAIN_GENERAL;

    if (!PyArg_ParseTuple(args, "s|i", &text, &domain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_learn_from_text: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (domain < KNOWLEDGE_DOMAIN_LANGUAGE || domain > KNOWLEDGE_DOMAIN_GENERAL) {
        PyErr_SetString(PyExc_ValueError, "Invalid domain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_learn_from_text: validation failed");
        return NULL;
    }

    uint32_t concepts_learned;

    Py_BEGIN_ALLOW_THREADS
    concepts_learned = knowledge_learn_from_text(self->system, text, (knowledge_domain_t)domain);
    Py_END_ALLOW_THREADS

    return PyLong_FromUnsignedLong(concepts_learned);
}

/**
 * @brief KnowledgeSystem.retrieve(concept) -> KnowledgeItem or None
 *
 * Retrieve knowledge about a concept.
 */
static PyObject* KnowledgeSystem_retrieve(KnowledgeSystemObject* self, PyObject* args)
{
    const char* concept;

    if (!PyArg_ParseTuple(args, "s", &concept)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_retrieve: PyArg_ParseTuple is NULL");
        return NULL;
    }

    knowledge_item_t item;
    memset(&item, 0, sizeof(item));

    bool found;

    Py_BEGIN_ALLOW_THREADS
    found = knowledge_retrieve(self->system, concept, &item);
    Py_END_ALLOW_THREADS

    if (!found) {
        Py_RETURN_NONE;
    }

    return KnowledgeItem_FromC(&item);
}

/**
 * @brief KnowledgeSystem.understand(concept, context) -> str
 *
 * Get contextual understanding of a concept.
 */
static PyObject* KnowledgeSystem_understand(KnowledgeSystemObject* self, PyObject* args)
{
    const char* concept;
    const char* context;

    if (!PyArg_ParseTuple(args, "ss", &concept, &context)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_understand: PyArg_ParseTuple is NULL");
        return NULL;
    }

    char explanation[NIMCP_JSON_BUFFER_SIZE];
    uint32_t length;

    Py_BEGIN_ALLOW_THREADS
    length = knowledge_understand(self->system, concept, context, explanation, sizeof(explanation));
    Py_END_ALLOW_THREADS

    if (length == 0) {
        Py_RETURN_NONE;
    }

    return PyUnicode_FromStringAndSize(explanation, (Py_ssize_t)length);
}

/**
 * @brief KnowledgeSystem.explain_simply(concept, target_age) -> str
 *
 * Explain concept at a target age level (3-18).
 */
static PyObject* KnowledgeSystem_explain_simply(KnowledgeSystemObject* self, PyObject* args)
{
    const char* concept;
    unsigned int target_age = 10;

    if (!PyArg_ParseTuple(args, "s|I", &concept, &target_age)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_explain_simply: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (target_age < 3 || target_age > 18) {
        PyErr_SetString(PyExc_ValueError, "target_age must be between 3 and 18");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_explain_simply: validation failed");
        return NULL;
    }

    char explanation[NIMCP_JSON_BUFFER_SIZE];
    uint32_t length;

    Py_BEGIN_ALLOW_THREADS
    length = knowledge_explain_simply(self->system, concept, target_age, explanation, sizeof(explanation));
    Py_END_ALLOW_THREADS

    if (length == 0) {
        Py_RETURN_NONE;
    }

    return PyUnicode_FromStringAndSize(explanation, (Py_ssize_t)length);
}

/**
 * @brief KnowledgeSystem.find_connections(concept, max_connections) -> list[KnowledgeItem]
 *
 * Find cross-domain connections for a concept.
 */
static PyObject* KnowledgeSystem_find_connections(KnowledgeSystemObject* self, PyObject* args)
{
    const char* concept;
    unsigned int max_connections = 10;

    if (!PyArg_ParseTuple(args, "s|I", &concept, &max_connections)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_find_connections: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (max_connections == 0 || max_connections > 100) {
        max_connections = 10;
    }

    knowledge_item_t* connections = (knowledge_item_t*)nimcp_calloc(max_connections, sizeof(knowledge_item_t));
    if (connections == NULL) {
        PyErr_NoMemory();
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_find_connections: validation failed");
        return NULL;
    }

    uint32_t num_found;

    Py_BEGIN_ALLOW_THREADS
    num_found = knowledge_find_connections(self->system, concept, connections, max_connections);
    Py_END_ALLOW_THREADS

    PyObject* result = PyList_New((Py_ssize_t)num_found);
    if (result == NULL) {
        nimcp_free(connections);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "KnowledgeSystem_find_connections: validation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < num_found; i++) {
        PyObject* item = KnowledgeItem_FromC(&connections[i]);
        if (item == NULL) {
            Py_DECREF(result);
            nimcp_free(connections);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_find_connections: validation failed");
            return NULL;
        }
        PyList_SET_ITEM(result, (Py_ssize_t)i, item);
    }

    nimcp_free(connections);
    return result;
}

/**
 * @brief KnowledgeSystem.build_on(new_concept, based_on, differences) -> bool
 *
 * Learn new concept by building on existing knowledge.
 */
static PyObject* KnowledgeSystem_build_on(KnowledgeSystemObject* self, PyObject* args)
{
    const char* new_concept;
    const char* based_on;
    const char* differences;

    if (!PyArg_ParseTuple(args, "sss", &new_concept, &based_on, &differences)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "KnowledgeSystem_build_on: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success;

    Py_BEGIN_ALLOW_THREADS
    success = knowledge_build_on(self->system, new_concept, based_on, differences);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(success);
}

/**
 * @brief KnowledgeSystem.reinforce(concept, example) -> bool
 *
 * Reinforce understanding with a new example.
 */
static PyObject* KnowledgeSystem_reinforce(KnowledgeSystemObject* self, PyObject* args)
{
    const char* concept;
    const char* example;

    if (!PyArg_ParseTuple(args, "ss", &concept, &example)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_reinforce: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success;

    Py_BEGIN_ALLOW_THREADS
    success = knowledge_reinforce(self->system, concept, example);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(success);
}

/**
 * @brief KnowledgeSystem.assess_domain(domain) -> DomainKnowledge
 *
 * Assess knowledge coverage in a domain.
 */
static PyObject* KnowledgeSystem_assess_domain(KnowledgeSystemObject* self, PyObject* args)
{
    int domain;

    if (!PyArg_ParseTuple(args, "i", &domain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_assess_domain: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (domain < KNOWLEDGE_DOMAIN_LANGUAGE || domain > KNOWLEDGE_DOMAIN_GENERAL) {
        PyErr_SetString(PyExc_ValueError, "Invalid domain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_assess_domain: validation failed");
        return NULL;
    }

    domain_knowledge_t assessment;
    memset(&assessment, 0, sizeof(assessment));

    bool success;

    Py_BEGIN_ALLOW_THREADS
    success = knowledge_assess_domain(self->system, (knowledge_domain_t)domain, &assessment);
    Py_END_ALLOW_THREADS

    if (!success) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to assess domain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "success is NULL");


        return NULL;
    }

    return DomainKnowledge_FromC(&assessment);
}

/**
 * @brief KnowledgeSystem.get_summary() -> list[DomainKnowledge]
 *
 * Get knowledge summary across all domains.
 */
static PyObject* KnowledgeSystem_get_summary(KnowledgeSystemObject* self, PyObject* Py_UNUSED(args))
{
    domain_knowledge_t assessments[11];  /* One per domain */
    memset(assessments, 0, sizeof(assessments));

    uint32_t num_domains;

    Py_BEGIN_ALLOW_THREADS
    num_domains = knowledge_get_summary(self->system, assessments, 11);
    Py_END_ALLOW_THREADS

    PyObject* result = PyList_New((Py_ssize_t)num_domains);
    if (result == NULL) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;
    }

    for (uint32_t i = 0; i < num_domains; i++) {
        PyObject* dk = DomainKnowledge_FromC(&assessments[i]);
        if (dk == NULL) {
            Py_DECREF(result);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_get_summary: validation failed");
            return NULL;
        }
        PyList_SET_ITEM(result, (Py_ssize_t)i, dk);
    }

    return result;
}

/**
 * @brief KnowledgeSystem.save(filepath) -> bool
 *
 * Save knowledge to persistent storage.
 */
static PyObject* KnowledgeSystem_save(KnowledgeSystemObject* self, PyObject* args)
{
    const char* filepath;

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_save: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success;

    Py_BEGIN_ALLOW_THREADS
    success = knowledge_save(self->system, filepath);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(success);
}

/**
 * @brief KnowledgeSystem.load(filepath) -> KnowledgeSystem (classmethod)
 *
 * Load knowledge from file.
 */
static PyObject* KnowledgeSystem_load(PyTypeObject* cls, PyObject* args)
{
    const char* filepath;
    (void)cls;  /* Unused */

    if (!PyArg_ParseTuple(args, "s", &filepath)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_load: PyArg_ParseTuple is NULL");
        return NULL;
    }

    knowledge_system_t system;

    Py_BEGIN_ALLOW_THREADS
    system = knowledge_load(filepath);
    Py_END_ALLOW_THREADS

    if (system == NULL) {
        PyErr_SetString(PyExc_IOError, "Failed to load knowledge system");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_load: validation failed");
        return NULL;
    }

    /* Create new object and assign loaded system */
    KnowledgeSystemObject* obj = PyObject_New(KnowledgeSystemObject, &KnowledgeSystemType);
    if (obj == NULL) {
        knowledge_system_destroy(system);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "KnowledgeSystem_load: validation failed");
        return NULL;
    }

    obj->system = system;
    return (PyObject*)obj;
}

/**
 * @brief KnowledgeSystem.organize_domain(domain) -> bool
 *
 * Organize knowledge into mental models.
 */
static PyObject* KnowledgeSystem_organize_domain(KnowledgeSystemObject* self, PyObject* args)
{
    int domain;

    if (!PyArg_ParseTuple(args, "i", &domain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_organize_domain: PyArg_ParseTuple is NULL");
        return NULL;
    }

    bool success;

    Py_BEGIN_ALLOW_THREADS
    success = knowledge_organize_domain(self->system, (knowledge_domain_t)domain);
    Py_END_ALLOW_THREADS

    return PyBool_FromLong(success);
}

/**
 * @brief KnowledgeSystem.get_by_confidence_range(min_conf, max_conf) -> list[KnowledgeItem]
 *
 * Query items within confidence range.
 */
static PyObject* KnowledgeSystem_get_by_confidence_range(KnowledgeSystemObject* self, PyObject* args)
{
    float min_conf, max_conf;

    if (!PyArg_ParseTuple(args, "ff", &min_conf, &max_conf)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_get_by_confidence_range: PyArg_ParseTuple is NULL");
        return NULL;
    }

    if (min_conf < 0.0f || max_conf > 1.0f || min_conf > max_conf) {
        PyErr_SetString(PyExc_ValueError, "Invalid confidence range (must be 0.0-1.0)");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_get_by_confidence_range: validation failed");
        return NULL;
    }

    knowledge_item_t* results = NULL;
    uint32_t num_results;

    Py_BEGIN_ALLOW_THREADS
    num_results = knowledge_get_by_confidence_range(self->system, min_conf, max_conf, &results);
    Py_END_ALLOW_THREADS

    PyObject* result_list = PyList_New((Py_ssize_t)num_results);
    if (result_list == NULL) {
        nimcp_free(results);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "KnowledgeSystem_get_by_confidence_range: validation failed");
        return NULL;
    }

    for (uint32_t i = 0; i < num_results; i++) {
        PyObject* item = KnowledgeItem_FromC(&results[i]);
        if (item == NULL) {
            Py_DECREF(result_list);
            nimcp_free(results);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_get_by_confidence_range: validation failed");
            return NULL;
        }
        PyList_SET_ITEM(result_list, (Py_ssize_t)i, item);
    }

    nimcp_free(results);
    return result_list;
}

/**
 * @brief KnowledgeSystem.domain_name(domain) -> str (staticmethod)
 *
 * Get human-readable name for a domain.
 */
static PyObject* KnowledgeSystem_domain_name(PyObject* Py_UNUSED(self), PyObject* args)
{
    int domain;

    if (!PyArg_ParseTuple(args, "i", &domain)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_domain_name: PyArg_ParseTuple is NULL");
        return NULL;
    }

    const char* name = knowledge_domain_name((knowledge_domain_t)domain);
    if (name == NULL) {
        Py_RETURN_NONE;
    }

    return PyUnicode_FromString(name);
}

/**
 * @brief KnowledgeSystem.query(query_str) -> str
 *
 * Query the knowledge graph and return a text result.
 * Mirrors the public nimcp_knowledge_query() API.
 */
static PyObject* KnowledgeSystem_query(KnowledgeSystemObject* self, PyObject* args)
{
    const char* query_str;

    if (!PyArg_ParseTuple(args, "s", &query_str)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_query: PyArg_ParseTuple failed");
        return NULL;
    }

    if (!self->system) {
        PyErr_SetString(PyExc_RuntimeError, "KnowledgeSystem not initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "KnowledgeSystem_query: self->system is NULL");
        return NULL;
    }

    knowledge_item_t item;
    memset(&item, 0, sizeof(item));

    bool found;

    Py_BEGIN_ALLOW_THREADS
    found = knowledge_retrieve(self->system, query_str, &item);
    Py_END_ALLOW_THREADS

    if (found) {
        return PyUnicode_FromString(item.definition);
    } else {
        return PyUnicode_FromFormat("No knowledge found about '%s'", query_str);
    }
}

static PyObject* KnowledgeSystem_repr(KnowledgeSystemObject* self)
{
    (void)self;
    return PyUnicode_FromString("KnowledgeSystem()");
}

static PyMethodDef KnowledgeSystem_methods[] = {
    {"learn_from_text", (PyCFunction)KnowledgeSystem_learn_from_text, METH_VARARGS,
     "Learn concepts from text\n\n"
     "Args:\n"
     "    text (str): Text to learn from\n"
     "    domain (int, optional): Knowledge domain (default: DOMAIN_GENERAL)\n"
     "Returns:\n"
     "    int: Number of concepts learned"},
    {"retrieve", (PyCFunction)KnowledgeSystem_retrieve, METH_VARARGS,
     "Retrieve knowledge about a concept\n\n"
     "Args:\n"
     "    concept (str): Concept to retrieve\n"
     "Returns:\n"
     "    KnowledgeItem or None: Knowledge item if found"},
    {"understand", (PyCFunction)KnowledgeSystem_understand, METH_VARARGS,
     "Get contextual understanding of a concept\n\n"
     "Args:\n"
     "    concept (str): Concept to understand\n"
     "    context (str): Context for understanding\n"
     "Returns:\n"
     "    str or None: Explanation in context"},
    {"explain_simply", (PyCFunction)KnowledgeSystem_explain_simply, METH_VARARGS,
     "Explain concept at a target age level\n\n"
     "Args:\n"
     "    concept (str): Concept to explain\n"
     "    target_age (int, optional): Target age 3-18 (default: 10)\n"
     "Returns:\n"
     "    str or None: Simple explanation"},
    {"find_connections", (PyCFunction)KnowledgeSystem_find_connections, METH_VARARGS,
     "Find cross-domain connections\n\n"
     "Args:\n"
     "    concept (str): Central concept\n"
     "    max_connections (int, optional): Maximum connections (default: 10)\n"
     "Returns:\n"
     "    list[KnowledgeItem]: Connected concepts"},
    {"build_on", (PyCFunction)KnowledgeSystem_build_on, METH_VARARGS,
     "Learn new concept by building on existing knowledge\n\n"
     "Args:\n"
     "    new_concept (str): New thing to learn\n"
     "    based_on (str): What it's similar to\n"
     "    differences (str): How it differs\n"
     "Returns:\n"
     "    bool: Success"},
    {"reinforce", (PyCFunction)KnowledgeSystem_reinforce, METH_VARARGS,
     "Reinforce understanding with a new example\n\n"
     "Args:\n"
     "    concept (str): Concept to reinforce\n"
     "    example (str): New example\n"
     "Returns:\n"
     "    bool: Success"},
    {"assess_domain", (PyCFunction)KnowledgeSystem_assess_domain, METH_VARARGS,
     "Assess knowledge coverage in a domain\n\n"
     "Args:\n"
     "    domain (int): Knowledge domain\n"
     "Returns:\n"
     "    DomainKnowledge: Assessment results"},
    {"get_summary", (PyCFunction)KnowledgeSystem_get_summary, METH_NOARGS,
     "Get knowledge summary across all domains\n\n"
     "Returns:\n"
     "    list[DomainKnowledge]: Assessment for each domain"},
    {"save", (PyCFunction)KnowledgeSystem_save, METH_VARARGS,
     "Save knowledge to persistent storage\n\n"
     "Args:\n"
     "    filepath (str): Path to save to\n"
     "Returns:\n"
     "    bool: Success"},
    {"load", (PyCFunction)KnowledgeSystem_load, METH_VARARGS | METH_CLASS,
     "Load knowledge from file (classmethod)\n\n"
     "Args:\n"
     "    filepath (str): Path to load from\n"
     "Returns:\n"
     "    KnowledgeSystem: Loaded system"},
    {"organize_domain", (PyCFunction)KnowledgeSystem_organize_domain, METH_VARARGS,
     "Organize knowledge into mental models\n\n"
     "Args:\n"
     "    domain (int): Domain to organize\n"
     "Returns:\n"
     "    bool: Success"},
    {"get_by_confidence_range", (PyCFunction)KnowledgeSystem_get_by_confidence_range, METH_VARARGS,
     "Query items within confidence range\n\n"
     "Args:\n"
     "    min_conf (float): Minimum confidence (0.0-1.0)\n"
     "    max_conf (float): Maximum confidence (0.0-1.0)\n"
     "Returns:\n"
     "    list[KnowledgeItem]: Items in range"},
    {"domain_name", (PyCFunction)KnowledgeSystem_domain_name, METH_VARARGS | METH_STATIC,
     "Get human-readable name for a domain (staticmethod)\n\n"
     "Args:\n"
     "    domain (int): Domain constant\n"
     "Returns:\n"
     "    str: Domain name"},
    {"query", (PyCFunction)KnowledgeSystem_query, METH_VARARGS,
     "Query the knowledge graph\n\n"
     "Returns a text result with the definition if the concept is found,\n"
     "or a 'not found' message otherwise.\n\n"
     "Args:\n"
     "    query_str (str): Concept or question to query\n"
     "Returns:\n"
     "    str: Text result\n\n"
     "Example:\n"
     "    result = ks.query('photosynthesis')\n"
     "    print(result)"},
    {NULL}
};

PyTypeObject KnowledgeSystemType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "nimcp.KnowledgeSystem",
    .tp_doc = PyDoc_STR(
        "Multi-domain knowledge acquisition system.\n\n"
        "Learn like a human - incrementally across domains like literature,\n"
        "science, ethics, history, and more.\n\n"
        "Args:\n"
        "    learner_name (str, optional): Name for the learner\n\n"
        "Example:\n"
        "    >>> ks = nimcp.KnowledgeSystem('my_learner')\n"
        "    >>> ks.learn_from_text('The cat sat on the mat.', nimcp.DOMAIN_LANGUAGE)\n"
        "    3\n"
        "    >>> item = ks.retrieve('cat')\n"
        "    >>> print(item.definition)"
    ),
    .tp_basicsize = sizeof(KnowledgeSystemObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = KnowledgeSystem_new,
    .tp_init = (initproc)KnowledgeSystem_init,
    .tp_dealloc = (destructor)KnowledgeSystem_dealloc,
    .tp_repr = (reprfunc)KnowledgeSystem_repr,
    .tp_methods = KnowledgeSystem_methods,
};

/* ============================================================================
 * Module initialization
 * ============================================================================ */

int init_knowledge_module(PyObject* module)
{
    LOG_MODULE_INFO("bindings.python.knowledge", "Initializing knowledge module");

    /* Ready types */
    if (PyType_Ready(&KnowledgeItemType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_knowledge_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&DomainKnowledgeType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_knowledge_module: validation failed");
        return -1;
    }
    if (PyType_Ready(&KnowledgeSystemType) < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_knowledge_module: validation failed");
        return -1;
    }

    /* Add types */
    Py_INCREF(&KnowledgeItemType);
    if (PyModule_AddObject(module, "KnowledgeItem", (PyObject*)&KnowledgeItemType) < 0) {
        Py_DECREF(&KnowledgeItemType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_knowledge_module: validation failed");
        return -1;
    }

    Py_INCREF(&DomainKnowledgeType);
    if (PyModule_AddObject(module, "DomainKnowledge", (PyObject*)&DomainKnowledgeType) < 0) {
        Py_DECREF(&DomainKnowledgeType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_knowledge_module: validation failed");
        return -1;
    }

    Py_INCREF(&KnowledgeSystemType);
    if (PyModule_AddObject(module, "KnowledgeSystem", (PyObject*)&KnowledgeSystemType) < 0) {
        Py_DECREF(&KnowledgeSystemType);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "init_knowledge_module: validation failed");
        return -1;
    }

    /* Add domain constants */
    PyModule_AddIntConstant(module, "DOMAIN_LANGUAGE", KNOWLEDGE_DOMAIN_LANGUAGE);
    PyModule_AddIntConstant(module, "DOMAIN_LITERATURE", KNOWLEDGE_DOMAIN_LITERATURE);
    PyModule_AddIntConstant(module, "DOMAIN_ART", KNOWLEDGE_DOMAIN_ART);
    PyModule_AddIntConstant(module, "DOMAIN_ETHICS", KNOWLEDGE_DOMAIN_ETHICS);
    PyModule_AddIntConstant(module, "DOMAIN_HISTORY", KNOWLEDGE_DOMAIN_HISTORY);
    PyModule_AddIntConstant(module, "DOMAIN_SCIENCE", KNOWLEDGE_DOMAIN_SCIENCE);
    PyModule_AddIntConstant(module, "DOMAIN_MATHEMATICS", KNOWLEDGE_DOMAIN_MATHEMATICS);
    PyModule_AddIntConstant(module, "DOMAIN_SOCIAL", KNOWLEDGE_DOMAIN_SOCIAL);
    PyModule_AddIntConstant(module, "DOMAIN_TECHNICAL", KNOWLEDGE_DOMAIN_TECHNICAL);
    PyModule_AddIntConstant(module, "DOMAIN_PHILOSOPHY", KNOWLEDGE_DOMAIN_PHILOSOPHY);
    PyModule_AddIntConstant(module, "DOMAIN_GENERAL", KNOWLEDGE_DOMAIN_GENERAL);

    LOG_MODULE_INFO("bindings.python.knowledge", "Knowledge module initialized successfully");
    return 0;
}
