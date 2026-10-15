#ifndef __db_H_
#define __db_H
#include <sqlite3.h> 
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

bool db_get_name_by_person_id(const char *person_id, char *out_name);
bool db_update_face_id_by_person(const char *person_id, const char *face_id);
extern sqlite3 *db;

#endif