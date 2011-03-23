static PyObject*
automaton_search_iter_new(
	Automaton* automaton,
	PyObject* object,
	int start,
	int end,
	bool is_unicode
) {
	AutomatonSearchIter* iter;

	iter = (AutomatonSearchIter*)PyObject_New(AutomatonSearchIter, &automaton_search_iter_type);
	if (iter == NULL)
		return NULL;

	iter->automaton = automaton;
	iter->version	= automaton->version;
	iter->object	= object;
	iter->is_unicode = is_unicode;
	if (is_unicode)
		iter->data = PyUnicode_AS_UNICODE(object);
	else
		iter->data = PyBytes_AS_STRING(object);

	iter->state	= automaton->root;
	iter->output= NULL;
	iter->index	= start - 1;	// -1 because first instruction in next() increments index
	iter->end	= end;

	Py_INCREF(iter->automaton);
	Py_INCREF(iter->object);

	return (PyObject*)iter;
}

#define iter ((AutomatonSearchIter*)self)

static void
automaton_search_iter_del(PyObject* self) {
	Py_DECREF(iter->automaton);
	Py_DECREF(iter->object);
	PyObject_Del(self);
}


static PyObject*
automaton_search_iter_iter(PyObject* self) {
	Py_INCREF(self);
	return self;
}


static PyObject*
automaton_search_iter_next(PyObject* self) {
	if (iter->version != iter->automaton->version) {
		PyErr_SetString(PyExc_ValueError, "underlaying automaton has changed, iterator is not valid anymore");
		return NULL;
	}

return_output:
	if (iter->output and iter->output->eow) {
		TrieNode* node = iter->output;
		PyObject* tuple;
		switch (iter->automaton->kind) {
			case STORE_LENGTH:
			case STORE_INTS:
				tuple = Py_BuildValue("ii", iter->index, node->output.integer);
				break;

			case STORE_ANY:
				tuple = Py_BuildValue("iO", iter->index, node->output.object);
				break;

			default:
				PyErr_SetString(PyExc_ValueError, "inconsistent internal state!");
				return NULL;
		}

		// next element to output
		iter->output = iter->output->fail;

		// yield value
		return tuple;
	}
	else
		iter->index += 1;

	while (iter->index < iter->end) {
#define NEXT(byte) ahocorasick_next(iter->state, iter->automaton->root, (byte))
		if (iter->is_unicode) {
#ifndef Py_UNICODE_WIDE
			// UCS-2 - process 1 or 2 bytes
			const uint16_t w = ((uint16_t*)iter->data)[iter->index];
			iter->state = NEXT(w & 0xff);
			if (w > 0x00ff)
				iter->state = NEXT((w >> 8) & 0xff);
#else
			// UCS-4 - process 1, 2, 3 or 4 bytes
			const uint32_t w = ((uint32_t*)iter->data)[iter->index];
			iter->state = NEXT(w & 0xff);
			if (w < 0x00010000)
				iter->state = NEXT((w >> 8) & 0xff);
			if (w < 0x01000000)
				iter->state = NEXT((w >> 16) & 0xff);
			if (w > 0x00ffffff)
				iter->state = NEXT((w >> 24) & 0xff);
#endif
		}
		else {
			// process single char
			const uint8_t w = ((uint8_t*)(iter->data))[iter->index];
			iter->state = NEXT(w);
		}
#undef NEXT

		ASSERT(iter->state);

		if (iter->state->eow) {
			iter->output = iter->state;
			goto return_output;
		}
		else
			iter->index += 1;

	} // while 
	
	return NULL;	// StopIteration
}

#undef iter

static PyTypeObject automaton_search_iter_type = {
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"AutomatonSearchIter",						/* tp_name */
	sizeof(AutomatonSearchIter),				/* tp_size */
	0,											/* tp_itemsize? */
	(destructor)automaton_search_iter_del,		/* tp_dealloc */
	0,                                      	/* tp_print */
	0,                                         	/* tp_getattr */
	0,                                          /* tp_setattr */
	0,                                          /* tp_reserved */
	0,											/* tp_repr */
	0,                                          /* tp_as_number */
	0,                                          /* tp_as_sequence */
	0,                                          /* tp_as_mapping */
	0,                                          /* tp_hash */
	0,                                          /* tp_call */
	0,                                          /* tp_str */
	PyObject_GenericGetAttr,                    /* tp_getattro */
	0,                                          /* tp_setattro */
	0,                                          /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,                         /* tp_flags */
	0,                                          /* tp_doc */
	0,                                          /* tp_traverse */
	0,                                          /* tp_clear */
	0,                                          /* tp_richcompare */
	0,                                          /* tp_weaklistoffset */
	automaton_search_iter_iter,					/* tp_iter */
	automaton_search_iter_next,					/* tp_iternext */
	0,											/* tp_methods */
	0,						                	/* tp_members */
	0,                                          /* tp_getset */
	0,                                          /* tp_base */
	0,                                          /* tp_dict */
	0,                                          /* tp_descr_get */
	0,                                          /* tp_descr_set */
	0,                                          /* tp_dictoffset */
	0,                                          /* tp_init */
	0,                                          /* tp_alloc */
	0,                                          /* tp_new */
};