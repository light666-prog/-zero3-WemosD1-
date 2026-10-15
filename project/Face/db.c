#include "db.h" 

sqlite3 *db = NULL;
bool db_get_name_by_person_id(const char *person_id, char *out_name) {
     if (!db) return false;

    // ★ 明确只取 name 列
    const char *sql = "SELECT name FROM SignPeople WHERE person_id=?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "prepare 失败: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, person_id, -1, SQLITE_STATIC);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        if (name) {
            strcpy(out_name, (const char*)name);
            found = true;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}
bool db_update_face_id_by_person(const char *person_id, const char *face_id) {
    if (!db) return false;
    const char *sql = "UPDATE SignPeople SET face_id=? WHERE person_id=?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, face_id,   -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, person_id, -1, SQLITE_STATIC);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}