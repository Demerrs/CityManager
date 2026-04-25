AI Used: Gemini Pro
The prompt given: The old code structure, the function names, a short description of what each function must do.
The result:

int parse_condition(const char *input, char *field, char *op, char *value) {
    // Folosim sscanf cu un format special: 
    // %[^:] inseamna "citeste toate caracterele pana intalnesti simbolul doua puncte ':'"
    // Am limitat citirea la 31, 3 si respectiv 99 caractere pentru a preveni buffer overflow
    int parsed = sscanf(input, "%31[^:]:%3[^:]:%99[^\n]", field, op, value);
    
    if (parsed == 3) {
        return 1; // Succes: am extras corect field, operator si valoare
    }
    return 0; // Eroare: formatul stringului nu este corect
}

int match_condition(ReportRecord *r, const char *field, const char *op, const char *value) {
    // ----------------------------------------------------
    // Cazul 1: Comparatii Numerice (severity, timestamp)
    // ----------------------------------------------------
    if (strcmp(field, "severity") == 0 || strcmp(field, "timestamp") == 0) {
        long val_rec = 0;
        long val_cond = atol(value); // Convertim valoarea din text in numar (long)

        // Extragem valoarea din structura in functie de campul cerut
        if (strcmp(field, "severity") == 0) {
            val_rec = r->severity_level;
        } else if (strcmp(field, "timestamp") == 0) {
            val_rec = (long)r->timestamp;
        }

        // Aplicam operatorul
        if (strcmp(op, "==") == 0) return val_rec == val_cond;
        if (strcmp(op, "!=") == 0) return val_rec != val_cond;
        if (strcmp(op, "<") == 0)  return val_rec < val_cond;
        if (strcmp(op, "<=") == 0) return val_rec <= val_cond;
        if (strcmp(op, ">") == 0)  return val_rec > val_cond;
        if (strcmp(op, ">=") == 0) return val_rec >= val_cond;
    }
    // ----------------------------------------------------
    // Cazul 2: Comparatii String (category, inspector)
    // ----------------------------------------------------
    else if (strcmp(field, "category") == 0 || strcmp(field, "inspector") == 0) {
        const char *str_rec = NULL;

        // Extragem pointer-ul catre string-ul din structura
        if (strcmp(field, "category") == 0) {
            str_rec = r->issue_category;
        } else if (strcmp(field, "inspector") == 0) {
            str_rec = r->inspector_name;
        }

        // Folosim strcmp care returneaza 0 daca sunt egale, <0 daca primul e mai mic, etc.
        int cmp = strcmp(str_rec, value);

        if (strcmp(op, "==") == 0) return cmp == 0;
        if (strcmp(op, "!=") == 0) return cmp != 0;
        if (strcmp(op, "<") == 0)  return cmp < 0;
        if (strcmp(op, "<=") == 0) return cmp <= 0;
        if (strcmp(op, ">") == 0)  return cmp > 0;
        if (strcmp(op, ">=") == 0) return cmp >= 0;
    }

    // Daca campul nu exista sau operatorul este invalid
    return 0;
}

Fixes:

- Refactored the structure, split into other smaller functions.
- Changed if else to thernary operator.
From:
long val_rec = 0;
if (strcmp(field, "severity") == 0) {
    val_rec = r->severity_level;
} else if (strcmp(field, "timestamp") == 0) {
    val_rec = (long)r->timestamp;
}

to: long val_rec = (strcmp(field, "severity") == 0) ? r->severity_level : (long)r->timestamp;
- Removed the <=, <, >=, > for string operations
- Put the reading the condition inside if:
Previously:
int parsed = sscanf(input, "%31[^:]:%3[^:]:%99[^\n]", field, op, value);
if (parsed == 3) { return 1; }

Now: 
if (sscanf(input, "%31[^:]:%3[^:]:%99[^\n]", field, op, value) == 3) {
    return 1;
}
