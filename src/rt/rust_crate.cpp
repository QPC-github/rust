
#include "rust_internal.h"

uintptr_t
rust_crate::get_image_base() const {
  return ((uintptr_t)this + image_base_off);
}

ptrdiff_t
rust_crate::get_relocation_diff() const {
  return ((uintptr_t)this - self_addr);
}

activate_glue_ty
rust_crate::get_activate_glue() const {
  return (activate_glue_ty) ((uintptr_t)this + activate_glue_off);
}

uintptr_t
rust_crate::get_exit_task_glue() const {
  return ((uintptr_t)this + exit_task_glue_off);
}

uintptr_t
rust_crate::get_unwind_glue() const {
  return ((uintptr_t)this + unwind_glue_off);
}

uintptr_t
rust_crate::get_gc_glue() const {
  return ((uintptr_t)this + gc_glue_off);
}

uintptr_t
rust_crate::get_yield_glue() const {
  return ((uintptr_t)this + yield_glue_off);
}

rust_crate::mem_area::mem_area(rust_dom *dom, uintptr_t pos, size_t sz)
  : dom(dom),
    base(pos),
    lim(pos + sz)
{
  DLOG(dom, rust_log::MEM, "new mem_area [0x%" PRIxPTR ",0x%" PRIxPTR "]",
           base, lim);
}

rust_crate::mem_area
rust_crate::get_debug_info(rust_dom *dom) const {
    if (debug_info_off)
        return mem_area(dom,
                        ((uintptr_t)this + debug_info_off),
                        debug_info_sz);
    else
        return mem_area(dom, 0, 0);
}

rust_crate::mem_area
rust_crate::get_debug_abbrev(rust_dom *dom) const {
    if (debug_abbrev_off)
        return mem_area(dom,
                        ((uintptr_t)this + debug_abbrev_off),
                        debug_abbrev_sz);
    else
        return mem_area(dom, 0, 0);
}

struct mod_entry {
    const char* name;
    int* state;
};

struct cratemap {
    mod_entry* entries;
    cratemap* children[1];
};

struct log_directive {
    char* name;
    size_t level;
};

const size_t max_log_directives = 255;

size_t parse_logging_spec(char* spec, log_directive* dirs) {
    size_t dir = 0;
    while (dir < max_log_directives && *spec) {
        char* start = spec;
        char cur;
        while (true) {
            cur = *spec;
            if (cur == ',' || cur == '=' || cur == '\0') {
                if (start == spec) {spec++; break;}
                *spec = '\0';
                spec++;
                size_t level = 3;
                if (cur == '=') {
                    level = *spec - '0';
                    if (level > 3) level = 1;
                    if (*spec) ++spec;
                }
                dirs[dir].name = start;
                dirs[dir++].level = level;
                break;
            }
            spec++;
        }
    }
    return dir;
}

void update_crate_map(cratemap* map, log_directive* dirs, size_t n_dirs) {
    // First update log levels for this crate
    for (mod_entry* cur = map->entries; cur->name; cur++) {
        size_t level = 1, longest_match = 0;
        for (size_t d = 0; d < n_dirs; d++) {
            if (strstr(cur->name, dirs[d].name) == cur->name &&
                strlen(dirs[d].name) > longest_match) {
                longest_match = strlen(dirs[d].name);
                level = dirs[d].level;
            }
        }
        *cur->state = level;
    }

    // Then recurse on linked crates
    for (size_t i = 0; map->children[i]; i++) {
        update_crate_map(map->children[i], dirs, n_dirs);
    }
}

void rust_crate::update_log_settings(char* settings) const {
    // Only try this if the crate was generated by Rustc, not rustboot
    if (image_base_off) return;

    // This is a rather ugly parser for strings in the form
    // "crate1,crate2.mod3,crate3.x=2". Log levels range 0=err, 1=warn,
    // 2=info, 3=debug. Default is 1. Words without an '=X' part set the log
    // level for that module (and submodules) to 3.
    char* buffer = NULL;
    log_directive dirs[256];
    size_t dir = 0;
    if (settings) {
        buffer = (char*)malloc(strlen(settings));
        strcpy(buffer, settings);
        dir = parse_logging_spec(buffer, &dirs[0]);
    }

    update_crate_map((cratemap*)crate_map, &dirs[0], dir);

    free(buffer);
}

//
// Local Variables:
// mode: C++
// fill-column: 78;
// indent-tabs-mode: nil
// c-basic-offset: 4
// buffer-file-coding-system: utf-8-unix
// compile-command: "make -k -C .. 2>&1 | sed -e 's/\\/x\\//x:\\//g'";
// End:
