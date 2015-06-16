#include <Python.h>
#include <limits.h>
#include <string.h>
#include <numpy/arrayobject.h>
#include <numpy/npy_math.h>

// Declaration of the voro++ wrapping function.
const char *hyperion_voropp_wrap(int **neighbours, int *max_nn, double **volumes, double **bb_min, double **bb_max, double **vertices, int *max_nv,
                  double xmin, double xmax, double ymin, double ymax, double zmin, double zmax,
                  double const *points, int npoints, int with_vertices, const char *wall_str, const double *wall_args_arr, int n_wall_args, int with_sampling,
                  int n_samples, double **sample_points, int **sampling_idx, int *tot_samples, int *min_cell_samples, int verbose);

/* Define docstrings */
static char module_docstring[] = "C implementation of utility functions used in Voronoi grids";
static char voropp_wrapper_docstring[] = "voro++ wrapper";

/* Declare the C functions here. */
static PyObject *_voropp_wrapper(PyObject *self, PyObject *args);

/* Define the methods that will be available on the module. */
static PyMethodDef module_methods[] = {
    {"_voropp_wrapper", _voropp_wrapper, METH_VARARGS, voropp_wrapper_docstring},
    {NULL, NULL, 0, NULL}
};

/* This is the function that is called on import. */

#if PY_MAJOR_VERSION >= 3
  #define MOD_ERROR_VAL NULL
  #define MOD_SUCCESS_VAL(val) val
  #define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          static struct PyModuleDef moduledef = { \
            PyModuleDef_HEAD_INIT, name, doc, -1, methods, }; \
          ob = PyModule_Create(&moduledef);
#else
  #define MOD_ERROR_VAL
  #define MOD_SUCCESS_VAL(val)
  #define MOD_INIT(name) void init##name(void)
  #define MOD_DEF(ob, name, doc, methods) \
          ob = Py_InitModule3(name, methods, doc);
#endif

MOD_INIT(_voronoi_core)
{
    PyObject *m;
    MOD_DEF(m, "_voronoi_core", module_docstring, module_methods);
    if (m == NULL)
        return MOD_ERROR_VAL;
    import_array();
    return MOD_SUCCESS_VAL(m);
}

static PyObject *_voropp_wrapper(PyObject *self, PyObject *args)
{
    PyObject *sites_obj, *domain_obj, *wall_args_obj;
    int with_vertices;
    const char *wall_str;
    int verbose;
    int with_sampling, n_samples, min_cell_samples;

    if (!PyArg_ParseTuple(args, "OOisOiiii", &sites_obj, &domain_obj, &with_vertices,&wall_str,&wall_args_obj,
        &with_sampling, &n_samples, &min_cell_samples, &verbose))
    {
        return NULL;
    }

    // Handle the wall-related arguments.
    // NOTE: at the moment, the walls implemented in voro++ have at most 7 doubles as construction params.
    double wall_args_arr[7];
    // The actual number of construction arguments.
    int n_wall_args = (int)PyTuple_GET_SIZE(wall_args_obj);
    if (n_wall_args > 7) {
        PyErr_SetString(PyExc_TypeError, "Too many construction arguments for the wall object.");
        return NULL;
    }
    {
        // Read the wall construction arguments.
        int i;
        for (i = 0; i < n_wall_args; ++i) {
            // NOTE: PyTuple_GetItem returns a borrowed reference, no need to handle refcount.
            wall_args_arr[i] = PyFloat_AS_DOUBLE(PyTuple_GetItem(wall_args_obj,(Py_ssize_t)i));
        }
    }

    /* Interpret the input objects as `numpy` arrays. */
    PyObject *s_array = PyArray_FROM_OTF(sites_obj, NPY_DOUBLE, NPY_IN_ARRAY);
    PyObject *d_array = PyArray_FROM_OTF(domain_obj, NPY_DOUBLE, NPY_IN_ARRAY);

    /* Handle invalid input. */
    if (s_array == NULL || d_array == NULL) {
        PyErr_SetString(PyExc_TypeError, "Invalid input objects.");
        Py_XDECREF(s_array);
        Py_XDECREF(d_array);
        return NULL;
    }

    double *s_data = (double*)PyArray_DATA(s_array);
    double *d_data = (double*)PyArray_DATA(d_array);

    int nsites = (int)PyArray_DIM(s_array, 0);

    double *volumes = NULL, *bb_min = NULL, *bb_max = NULL, *vertices = NULL, *sample_points = NULL;
    int *neighbours = NULL, *sampling_idx = NULL;
    int max_nn, max_nv, tot_samples;

    // Call the wrapper.
    const char *status = hyperion_voropp_wrap(&neighbours,&max_nn,&volumes,&bb_min,&bb_max,&vertices,&max_nv,
                                              d_data[0],d_data[1],d_data[2],d_data[3],d_data[4],d_data[5],s_data,nsites,with_vertices,
                                              wall_str,wall_args_arr,n_wall_args,with_sampling,n_samples,&sample_points,&sampling_idx,
                                              &tot_samples,&min_cell_samples,verbose
                                             );

    if (status != NULL) {
        PyErr_SetString(PyExc_RuntimeError, status);
        Py_XDECREF(s_array);
        Py_XDECREF(d_array);
        return NULL;
    }

    // Unfortunately, it seemse like there is no easy way to just re-use the memory we allocated in the wrapper to create a numpy array
    // without copy. See, e.g., here:
    // http://blog.enthought.com/python/numpy-arrays-with-pre-allocated-memory
    // We will just create new numpy arrays and return them for now.
    npy_intp vol_dims[] = {nsites};
    npy_intp neigh_dims[] = {nsites,max_nn};
    npy_intp bb_dims[] = {nsites,3};
    npy_intp vert_dims[] = {nsites,max_nv};
    npy_intp spoints_dims[] = {tot_samples,3};
    npy_intp spoints_idx_dims[] = {nsites+1};

    PyObject *vol_array = PyArray_SimpleNew(1,vol_dims,NPY_DOUBLE);
    PyObject *neigh_array = PyArray_SimpleNew(2,neigh_dims,NPY_INT);
    PyObject *bb_min_array = PyArray_SimpleNew(2,bb_dims,NPY_DOUBLE);
    PyObject *bb_max_array = PyArray_SimpleNew(2,bb_dims,NPY_DOUBLE);
    // NOTE: Py_BuildValue("") is just a safe way to construct None.
    PyObject *vert_array = with_vertices ? PyArray_SimpleNew(2,vert_dims,NPY_DOUBLE) : Py_BuildValue("");
    PyObject *spoints_array = with_sampling ? PyArray_SimpleNew(2,spoints_dims,NPY_DOUBLE) : Py_BuildValue("");
    PyObject *spoints_idx_array = with_sampling ? PyArray_SimpleNew(1,spoints_idx_dims,NPY_INT) : Py_BuildValue("");

    if (vol_array == NULL || neigh_array == NULL || bb_min_array == NULL || bb_max_array == NULL ||
        vert_array == NULL || spoints_array == NULL || spoints_idx_array == NULL)
    {
        PyErr_SetString(PyExc_MemoryError, "Memory allocation error.");
        free(neighbours);
        free(volumes);
        free(bb_min);
        free(bb_max);
        free(vertices);
        free(sample_points);
        free(sampling_idx);
        Py_XDECREF(s_array);
        Py_XDECREF(d_array);
        Py_XDECREF(vol_array);
        Py_XDECREF(neigh_array);
        Py_XDECREF(bb_min_array);
        Py_XDECREF(bb_max_array);
        Py_XDECREF(vert_array);
        Py_XDECREF(spoints_array);
        Py_XDECREF(spoints_idx_array);
        return NULL;
    }

    // Copy over the data.
    memcpy((double*)PyArray_DATA(vol_array),volumes,sizeof(double) * nsites);
    memcpy((int*)PyArray_DATA(neigh_array),neighbours,sizeof(int) * nsites * max_nn);
    memcpy((double*)PyArray_DATA(bb_min_array),bb_min,sizeof(double) * nsites * 3);
    memcpy((double*)PyArray_DATA(bb_max_array),bb_max,sizeof(double) * nsites * 3);
    if (with_vertices) {
        memcpy((double*)PyArray_DATA(vert_array),vertices,sizeof(double) * nsites * max_nv);
    }
    if (with_sampling) {
        memcpy((double*)PyArray_DATA(spoints_array),sample_points,sizeof(double) * tot_samples * 3);
        memcpy((int*)PyArray_DATA(spoints_idx_array),sampling_idx,sizeof(int) * (nsites + 1));
    }

    PyObject *retval = PyTuple_Pack(7,neigh_array,vol_array,bb_min_array,bb_max_array,vert_array,spoints_array,spoints_idx_array);

    // Final cleanup.
    free(neighbours);
    free(volumes);
    free(bb_min);
    free(bb_max);
    free(vertices);
    free(sample_points);
    free(sampling_idx);
    Py_XDECREF(s_array);
    Py_XDECREF(d_array);
    // NOTE: these need to be cleaned up as PyTuple_Pack will increment the reference count. See:
    // https://mail.python.org/pipermail/capi-sig/2009-February/000222.html
    Py_XDECREF(neigh_array);
    Py_XDECREF(vol_array);
    Py_XDECREF(bb_min_array);
    Py_XDECREF(bb_max_array);
    Py_XDECREF(vert_array);
    Py_XDECREF(spoints_array);
    Py_XDECREF(spoints_idx_array);

    return retval;
}