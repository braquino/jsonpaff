#include "jsonpaff.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#define MAX_NSEG_PATH 20
#define MAX_ELEM_LIST 300

enum ErrorCode {
    OBJECT_NOT_FOUND_ERROR = 1001,
    BAD_PATH_ERROR = 1011,
    NOT_IMPLEMENTED_ERROR = 1021,
    END_OF_OBJECT = 2001
};

typedef enum PathType {
    KEY, SUBSCRIPT, STAR, ANYLVL
} PathType;

typedef struct Substring {
    char *ptr;
    size_t len;
    char *tofree;
} Substring;

typedef struct PathSeg {
    Substring sub;
    PathType type;
} PathSeg;

typedef struct Path {
    PathSeg segs[MAX_NSEG_PATH];
    size_t len;
} Path;

int eqlSubstring(Substring a, Substring b) {
    if (a.len != b.len) {
        return 0;
    }
    for (int i=0; i<a.len; i++) {
        if (*(a.ptr + i) != *(b.ptr + i)) {
            return 0;
        }
    }
    return 1;
}

// Returns the complete next number found, must start from the first object's char
int getNextNumber(Substring obj, Substring *output) {
    char c = 0;
    for (size_t i = 0; i < obj.len; i++) {
        c = *(obj.ptr + i);
        if (c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '}' || c == ']') {
             output->ptr = obj.ptr;
             output->len = i;
             return 0;
        }
    }
    return OBJECT_NOT_FOUND_ERROR;
}

// Returns the complete next text found, must start from the first object's char
int getNextText(Substring obj, Substring *output) {
    char c = 0;
    for (size_t i = 0; i < obj.len; i++) {
        c = *(obj.ptr + i);
        if (c == '\\') {
            i++;
            continue;
        }
        if (c == '"' && i > 0) {
            output->ptr = obj.ptr;
            output->len = i + 1;
            return 0;
        }
    }
    return OBJECT_NOT_FOUND_ERROR;
}

// Returns the complete next object or array found, must start from the first object's char
int getNextObject(Substring obj, Substring *output) {
    int obj_open = 0;
    int8_t quoted = 0;

    char c = 0;
    for (size_t i = 0; i < obj.len; i++) {
        c = *(obj.ptr + i);
        if (!quoted) {
            if (c == '"') {
                quoted = 1;
                continue;
            }
            if (c == '{' || c == '[') {
                obj_open++;
                continue;
            }
            if (c == '}' || c == ']') {
                obj_open--;
                if (obj_open == 0) {
                    output->ptr = obj.ptr;
                    output->len = i + 1;
                    return 0;
                }
            }
        } else {
            if (c == '"') {
                quoted = 0;
            }
            if (c == '\\') {
                i += 1;
                continue;
            }
        }
    }
    return OBJECT_NOT_FOUND_ERROR;
}

// Returns the complete next element found, must start from the first object's char
int getNext(Substring obj, Substring *output) {
    int error = 0;
    if (*obj.ptr == '{' || *obj.ptr == '[') {
        error = getNextObject(obj, output);
        if (error != 0) {
            printf("error code '%d' in getNextObject\n", error);
        }
        return error;
    }
    if (*obj.ptr == '"') {
        error = getNextText(obj, output);
        if (error != 0) {
            printf("error code '%d' in getNextText\n", error);
        }
        return error;
    }
    error = getNextNumber(obj, output);
    if (error != 0) {
        printf("error code '%d' in getNextNumber\n", error);
    }
    return error;

}

// Returns a component of a list with the 'index' number
int getSubscript(Substring obj, int idx, Substring *output) {
    int error = 0;
    Substring cur_obj = {NULL, 0};
    int8_t init = 0;
    int cur_idx = 0;

    char c = 0;
    for (size_t i = 0; i < obj.len; i++) {
        c = *(obj.ptr + i);
        if (init) {
            if (c != ',' && c != ' ' && c != '\t' && c != '\n') {
                if ((error = getNext((Substring){obj.ptr + i, obj.len - i}, &cur_obj))) {
                    return error;
                }
                if (cur_idx == idx) {
                    output->ptr = cur_obj.ptr;
                    output->len = cur_obj.len;
                    return 0;
                } else {
                    cur_idx++;
                    i += cur_obj.len;
                    continue;
                }
            }
        } else {
            if (c == '[') {
                init = 1;
            }
        }
    }
    return OBJECT_NOT_FOUND_ERROR;
}

// 'output' an inner object with the 'prop' key, if error returns non zero
int getObject(Substring obj, Substring prop, Substring *output, int anylvl) {
    int obj_open = 0;
    int8_t quoted = 0;
    int8_t init = anylvl;
    int8_t found = 0;

    char c = 0;
    for (size_t i = 0; i < obj.len; i++) {
        if (obj_open < 0 && !anylvl) {
            return OBJECT_NOT_FOUND_ERROR;
        }
        c = *(obj.ptr + i);
        if (c == '\\') {
            i++;
            continue;
        }
        if (init) {
            if (found) {
                if (c != ':' && c != ' ' && c != '\t' && c != '\n') {
                    int error = getNext((Substring){obj.ptr + i, obj.len - i}, output);
                    return error;
                }
            } else {
                if (!quoted) {
                    if (c == '"') {
                        quoted = 1;
                        continue;
                    }
                    if (c == '{' || c == '[') {
                        obj_open++;
                    }
                    if (c == '}' || c == ']') {
                        obj_open--;
                    }
                }
                if (quoted) {
                    if (c == '"') {
                        quoted = 0;
                    }
                    if ((obj_open == 0 || anylvl) && c == *prop.ptr && *(obj.ptr + i - 1) == '"' && eqlSubstring((Substring){obj.ptr + i, prop.len}, prop) && *(obj.ptr + i + prop.len) == '"') {
                        found = 1;
                        i += prop.len;
                        continue;
                    }
                }
            }
        } else {
            if (c == '{' || c == '[') init = 1;
        }
    }
    return OBJECT_NOT_FOUND_ERROR;
}

void definePathType(PathSeg *seg) {
    if (seg->sub.len < 1) {
        return;
    }
    if (*(seg->sub.ptr) == '\'' && *(seg->sub.ptr + seg->sub.len - 1) == '\'') {
        seg->type = KEY;
        seg->sub.ptr++;
        seg->sub.len -= 2;
        return;
    }
    if (*(seg->sub.ptr) == '*') {
        seg->type = STAR;
        return;
    }
    if (isdigit(*(seg->sub.ptr))) {
        seg->type = SUBSCRIPT;
        return;
    }
    seg->type = KEY;
    return;
}

// Given a path, returns a NULL terminated array of PathSeg
int parsePath(Substring JSONPath, Path *output) {
    output->len = 0;
    if (*(JSONPath.ptr) != '$') {
        return BAD_PATH_ERROR;
    }
    int8_t quote_idx = 0;
    size_t last_seg_idx = 0;

    char c = 0;
    for (size_t i = 1; i < JSONPath.len; i++) {   
        c = *(JSONPath.ptr + i);
        if (c == '.' || c == '[') {
            if (c == '.' && *(JSONPath.ptr + i - 1) == '.') {
                output->segs[output->len] = (PathSeg){(Substring){JSONPath.ptr + last_seg_idx, 2}, ANYLVL};
                output->len++;
                last_seg_idx = i;
                continue;
            }
            if (quote_idx) {
                printf("path error: opening [ already oppened, idx: %ld\n", i);
                return BAD_PATH_ERROR;
            } else {
                if (c == '[') quote_idx = i;
                if (!last_seg_idx || last_seg_idx + 1 == i) {
                    last_seg_idx = i;
                    continue;
                } else {
                    output->segs[output->len] = (PathSeg){(Substring){JSONPath.ptr + last_seg_idx + 1, i - last_seg_idx - 1}, KEY};
                    definePathType(&(output->segs[output->len]));
                    output->len++;
                    last_seg_idx = i;
                    continue;
                }
            }
        }
        if (c == ']') {
            if (quote_idx) {
                output->segs[output->len] = (PathSeg){(Substring){JSONPath.ptr + last_seg_idx + 1, i - last_seg_idx - 1}, KEY};
                definePathType(&(output->segs[output->len]));
                output->len++;
                last_seg_idx = i;
                quote_idx = 0;
                continue;
            } else {
                printf("path error: closing ] not oppened, idx: %ld\n", i);
                return BAD_PATH_ERROR;
            }
        }
    }
    if (last_seg_idx < JSONPath.len - 1) {
        output->segs[output->len] = (PathSeg){(Substring){JSONPath.ptr + last_seg_idx + 1, JSONPath.len - last_seg_idx - 1}, KEY};
        output->len++;
    }
    return 0;
}

// add each obj to the string, return new idx
int makeList(const Substring obj, int stridx, char strbuffer[]) {
    if (stridx == 0) {
        strbuffer[0] = '[';
        stridx++;
    }

    if (obj.len == 0) {
        strbuffer[stridx] = ']';
        stridx++;
    } else {
        if (stridx > 1) {
            memcpy((char*)strbuffer + stridx, ", " , 2);
            stridx += 2;
        }
        memcpy((char*)strbuffer + stridx, obj.ptr, obj.len);
        stridx += obj.len;
    }

    strbuffer[stridx + 1] = 0;
    return stridx;
}

// Recursively finds and returns the next path segment
int recPath(Substring obj, Path *path, int path_idx, Substring *output) {
    int error = 0;
    int error_list = 0;
    if (path->len > path_idx) {
        if (path->segs[path_idx].type == KEY) {
            error = getObject(obj, path->segs[path_idx].sub, output, 0);
            if (error != 0) {
                printf("error on getObject, code: %d\n", error);
                return error;
            }
            return recPath(*output, path, path_idx+1, output);
        }
        if (path->segs[path_idx].type == SUBSCRIPT) {
            PathSeg seg = path->segs[path_idx];
            char subs_num[20];
            memcpy(subs_num, seg.sub.ptr, seg.sub.len);
            subs_num[seg.sub.len] = 0;
            int subs_idx = strtol(subs_num, NULL, 10);
            
            error = getSubscript(*output, subs_idx, output);
            if (error != 0) {
                printf("error on getSubscript, code: %d\n", error);
                return error;
            }
            return recPath(*output, path, path_idx+1, output);
        }
        if (path->segs[path_idx].type == ANYLVL) {
            char *strbuff = malloc(obj.len + 100);
            int n_el = 0;
            int stridx = 0;
            while ((error_list = getObject(obj, path->segs[path_idx + 1].sub, output, 1)) != OBJECT_NOT_FOUND_ERROR) {
                if (!error_list) {
                    error += recPath(*output, path, path_idx+2, output);
                }
                if (error_list) {
                    printf("error getObject making a list, code: %d\n", error);
                    break;
                }
                if (error) {
                    printf("error recPath making a list, code: %d\n", error);
                    break;
                }
                if (!error_list && !error) {
                    stridx = makeList(*output, stridx, strbuff);
                }
                obj.len -= ((output->ptr + output->len) - obj.ptr);
                obj.ptr += ((output->ptr + output->len) - obj.ptr);
            }
            if (error_list == OBJECT_NOT_FOUND_ERROR && !error) {
                stridx = makeList((Substring){NULL, 0}, stridx, strbuff);
            }
            output->ptr = strbuff;
            output->len = stridx;
            output->tofree = strbuff;
            
            return error;
        }
        return NOT_IMPLEMENTED_ERROR;
    } else {
        return 0;
    }
}

extern int getJSONPath(const char *obj, const char *JSONPath, char output[]) {
    int error = 0;
    Substring result = {NULL, 0};
    Path path;
    error = parsePath((Substring){(char*)JSONPath, strlen(JSONPath)}, &path);
    if (error != 0) {
        printf("error parsePath code: %d\n", error);
        return error;
    }
    error = recPath((Substring){(char*)obj, strlen(obj)}, &path, 0, &result);
    if (error != 0) {
        printf("error retriving object code: %d\n", error);
        return error;
    }

    memcpy(output, result.ptr, result.len);
    output[result.len] = 0;
    if (result.tofree) {
        free(result.tofree);
    }
    return 0;
}


// ######### PRIVATE TESTS ###########

void printSubstring(Substring s) {
    for (size_t i = 0; i < s.len; i++) {
        printf("%c", *(s.ptr + i));
    }
}

int eqlSubstringVerbose(Substring a, Substring b) {
    int compare;
    if (!(compare = eqlSubstring(a, b))) {
        printf("'");
        printSubstring(a);
        printf("' not equal '");
        printSubstring(b);
        printf("'\n");
    }
    return compare;
}

int test_eqlSubstring() {
    char *a = "hello my little friend";
    char *b = "bye my little";
    Substring sa = {a+6, 9};
    Substring sb = {b+4, 9};
    if (!eqlSubstring(sa, sb)) {
        printf("sa != sb\n");
        return 1;
    }
    return 0;
}

int test_getObject() {
    int error;
    char *j = "{\"foo\": 5, \"bar\": {\"hello\": 2}, \"hello\": 3 , \"text\": \"a_text\"}";
    char *k1 = "hello";
    char *k1_expected = "3";
    char *k2 = "bar";
    char *k2_expected = "{\"hello\": 2}";
    char *k3 = "text";
    char *k3_expected = "\"a_text\"";
    Substring obj = {j, strlen(j)};
    Substring output = {NULL, 0};

    error = getObject(obj, (Substring){k1, strlen(k1)}, &output, 0);
    assert(error == 0);
    assert(eqlSubstringVerbose(output, (Substring){k1_expected, strlen(k1_expected)}));
    error = getObject(obj, (Substring){k2, strlen(k2)}, &output, 0);
    assert(error == 0);
    assert(eqlSubstringVerbose(output, (Substring){k2_expected, strlen(k2_expected)}));
    error = getObject(obj, (Substring){k3, strlen(k3)}, &output, 0);
    assert(error == 0);
    assert(eqlSubstringVerbose(output, (Substring){k3_expected, strlen(k3_expected)}));
    return 0;
}

int test_parsePath() {
    Path p;
    int error;
    char *jpath = "$['foo'].bar[5].hello[*]";
    error = parsePath((Substring){jpath, strlen(jpath)}, &p);
    assert(error == 0);
    if (p.len != 5) {
        printf("%ld != 5\n", p.len);
        return 1;
    }
    assert(eqlSubstringVerbose((Substring){jpath + 3, 3}, p.segs[0].sub));
    assert(KEY == p.segs[0].type);
    assert(eqlSubstringVerbose((Substring){jpath + 9, 3}, p.segs[1].sub));
    assert(KEY == p.segs[1].type);
    assert(eqlSubstringVerbose((Substring){jpath + 13, 1}, p.segs[2].sub));
    assert(SUBSCRIPT == p.segs[2].type);
    assert(eqlSubstringVerbose((Substring){jpath + 16, 5}, p.segs[3].sub));
    assert(KEY == p.segs[3].type);
    assert(eqlSubstringVerbose((Substring){jpath + 22, 1}, p.segs[4].sub));
    assert(STAR == p.segs[4].type);

    char *jpath2 = "$..foo['bar']";
    error = parsePath((Substring){jpath2, strlen(jpath2)}, &p);
    assert(error == 0);
    if (p.len != 3) {
        printf("%ld != 3\n", p.len);
        return 1;
    }
    assert(eqlSubstringVerbose((Substring){jpath2 + 1, 2}, p.segs[0].sub));
    assert(ANYLVL == p.segs[0].type);
    assert(eqlSubstringVerbose((Substring){jpath2 + 3, 3}, p.segs[1].sub));
    assert(KEY == p.segs[1].type);
    assert(eqlSubstringVerbose((Substring){jpath2 + 8, 3}, p.segs[2].sub));
    assert(KEY == p.segs[2].type);

    return 0;
}

int test_getSubscript() {
    int error = 0;
    char* jstr = "[{\"bar\":2}, {\"bar\":3}, {\"bar\":4}]";
    Substring result = {NULL, 0};
    error = getSubscript((Substring){jstr, strlen(jstr)}, 1, &result);
    assert(error == 0);
    assert(eqlSubstringVerbose((Substring){jstr + 12, 9}, result));
    return 0;
}

int test_getJSONPath() {
    int error = 0;
    char* jstr = "{\"input\": {\"hash\": 0, \"forward_hashes\": [1]}, \
        \"processes\": [ \
          {\"name\": \"foo\", \"python\": \"print('hello')\", \"hash\": 1, \"forward_hashes\": [2]}, \
          {\"name\": \"bar\", \"python\": \"print('bie')\", \"hash\": 2, \"forward_hashes\": [2]}, \
          {\"name\": \"iou\", \"python\": \"print('ciao')\", \"hash\": 3, \"forward_hashes\": [3]} \
        ], \"outputs\": [{\"hash\": 2}]}";
    
    char result[2000];

    char *path_1 = "$.input.hash";
    error = getJSONPath(jstr, path_1, result);
    assert(error == 0);
    assert(strcmp(result, "0") == 0);

    char *path_2 = "$.processes[1]";
    error = getJSONPath(jstr, path_2, result);
    assert(error == 0);
    assert(strcmp(result, 
        "{\"name\": \"bar\", \"python\": \"print('bie')\", \"hash\": 2, \"forward_hashes\": [2]}") == 0);

    char *path_3 = "$.processes[2].forward_hashes[0]";
    error = getJSONPath(jstr, path_3, result);
    assert(error == 0);
    assert(strcmp(result, "3") == 0);

    char *path_4 = "$..hash";
    error = getJSONPath(jstr, path_4, result);
    assert(error == 0);
    assert(strcmp(result, "[0, 1, 2, 3, 2]") == 0);

    return 0;
}

int run_all_test() {
    int all_tests = 0;
    int fail = 0;

    fail = test_eqlSubstring();
    all_tests += fail;
    if (fail) printf("test_eqlSubstring failed\n");

    fail = test_getObject();
    all_tests += fail;
    if (fail) printf("test_getObject failed\n");

    fail = test_parsePath();
    all_tests += fail;
    if (fail) printf("test_parsePath failed\n");

    fail = test_getSubscript();
    all_tests += fail;
    if (fail) printf("test_getSubscript failed\n");

    fail = test_getJSONPath();
    all_tests += fail;
    if (fail) printf("test_getJSONPath failed\n");

    if (!all_tests) {
        printf("all tests OK\n");
        return 0;
    } else {
        printf("%d tests failed\n", all_tests);
        return 1;
    }
}

#ifdef PRIVATE_TESTS
int main() {
    return run_all_test();
}
#endif