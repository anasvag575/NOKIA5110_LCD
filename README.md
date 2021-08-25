# Swiss Hashtable

A C implementation of the Swiss hashtables provided by Abseil. A very good talk about the inner workings of hashtables in general and how Swiss tables operate and came to be [https://www.youtube.com/watch?v=ncHmEUmJZf4].

This library contains 2 versions of the containers, namely a **"flat"** and a **"node"** one, with each one being used for constant and variable sized keys for all the operations.

### Operation

This hashtables use open addressing for the layout of the hashtable and SSE instructions to speed up search/lookup operation. This library provides the following main operations and features:

- Lookup
- Insertion
- Delete
- Iteration
- Emplace (Only for the flat variant)
- Linear/Quadratic probing
- Multiple hashing functions

The hashtables automatically resize when entries are below 25% or above 75% of capacity (with the ranges being parameterized). Iteration has O(N) complexity while all the other operations have constant amortized cost of O(1).

#### Flat variant

This version stores the key and the payload of the user inside the hashtable, with the key and the payload having the limitation of being a constant size (i.e structs). This version is suited for small keys (ints or longs) and small payload sizes where locality is of great importance.

#### Node variant

This version stores a reference to the key and the payload, so the user has to allocate it himself beforehand. It supports variable sized key and variable sized payloads, so use that instead of the flat variant if there are those limitations. For this to happen, callback functions are used.

### Usage

The main API for both variants, where **xx** denotes the variant used (**flat**/**node**):

`void *ht_xx_search(xx_hashtable_t *hashtable, const void *key)`

Search operation, where given a key the entry/payload is searched. If found returns the reference to the entry, if not NULL.

`void *ht_xx_insert(xx_hashtable_t *hashtable, const void *key, const void *entry, int *error_code)`

Insertion operation, given a key and the entry/payload, tries to insert it inside the hashtable. If the key already exists then the operation fails and returns the existing entry. If the return is NULL, then the error code denotes the status of the operation with 0 being success.

`int ht_xx_delete(xx_hashtable_t *hashtable, const void *key)`

Delete operation, given a key, try to delete the entry associated with it. If the operation succeeds the return is 0, else an appropriate error code is returned.

`xx_hashtable_tuple_t <ht_xx_start_it/ht_xx_end_it>(xx_hashtable_t *hashtable)`

Returns a tuple of key and entry pointers to the start or the end of the hashtable. Also, initializes iteration status inside the hashtable. Iteration may be invalid if an insertion or delete is performed midway.

`xx_hashtable_tuple_t <ht_xx_next_it/ht_xx_prev_it>(xx_hashtable_t *hashtable)`

Returns a tuple of key and entry pointers to the next or the previous elements of the hashtable. Also, updates iteration status inside the hashtable. Iteration may be invalid if an insertion or delete is performed midway.

`void ht_xx_free(xx_hashtable_t *hashtable)`

Destroys the hashtable and the entries stored inside.

#### Flat specific

`flat_hashtable_t *ht_flat_create(size_t hashtable_sz, size_t entry_sz, size_t key_sz, int *error_code)`

Creates a flat hashtable with the keys having a size of *key_sz* and the entries *entry_sz*. An initial size for the hashtable can be specified as a hint, if the user knows the approximate amount of insertions. Returns the hashtable reference in case of success, else NULL and error code is set appropriately.

`int ht_flat_emplace(flat_hashtable_t *hashtable, const void *key, const void *entry)`

Emplace operation, which works like insertion but instead replaces the entry if the key already exists. In case of success 0 is returned, else the appropriate error code.

#### Node specific

```
node_hashtable_t *ht_node_create(size_t hashtable_sz, int (*comp)(const void *, const void *),
void (*destruct)(void *, void *), size_t (*hash)(const void *), int *error_code)
```

Creates a node hashtable, with the user providing callback functions for the operations with the entries. An initial size for the hashtable can be specified as a hint, if the user knows the approximate amount of insertions. Returns the hashtable reference in case of success, else NULL and error code is set appropriately.

### Testing

A *Makefile* is provided along with 2 different test files (test_int.c / test_str.c) for the flat/node variants. The *testcases/* folder contains dictionaries of different sizes.

To execute the tests, inside the folder with the Makefile and source code:

```
#Build the library and link with the testcases
make

#Tests the flat hashtable with multiple sizes of key/entries and shows usage of this variant
./test_int

#Tests the node hashtable with word dictionaries and shows usage of this variant
./test_str  testcases/<testcase_name>
```

For now the library has been tested only on multiple versions of Ubuntu - x86-64 architecture. Feel free to inform me, in case an issue is found.

