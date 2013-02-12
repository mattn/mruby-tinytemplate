#include <mruby.h>
#include <mruby/string.h>
#include <mruby/data.h>
#include <mruby/compile.h>
#include <mruby/class.h>
#include <mruby/hash.h>
#include <mruby/array.h>
#include <mruby/proc.h>
#include <mruby/variable.h>
#include "opcode.h"
#include <fstream>
#include <sstream>

typedef struct {
  mrb_state* mrb;
  std::string tmpl;
} mrb_tinytemplate;

static void
mrb_tinytemplate_free(mrb_state *mrb, void *p) {
  mrb_tinytemplate* tt = (mrb_tinytemplate*) p;
  mrb_close(tt->mrb);
  delete tt;
}

struct mrb_data_type mrb_tinytemplate_data_type = {
  "tinytemplate", mrb_tinytemplate_free 
};

static mrb_value
mrb_tinytemplate_init(mrb_state* mrb, mrb_value self) {
  mrb_value path;
  mrb_tinytemplate* tt;

  mrb_get_args(mrb, "S", &path);

  std::ifstream ifs;
  ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    ifs.open(mrb_str_to_cstr(mrb, path));
  } catch (std::ifstream::failure e) {
    mrb_raise(mrb, E_RUNTIME_ERROR, e.what());
  }
  std::stringstream ss;
  try {
    ss << '"';
    ss << ifs.rdbuf();    
    ss << '"';
    ifs.close();
  } catch (std::ifstream::failure e) {
    ifs.close();
    mrb_raise(mrb, E_RUNTIME_ERROR, e.what());
  }

  tt = new mrb_tinytemplate;
  tt->mrb = mrb_open(); 
  tt->tmpl = ss.str();

  DATA_TYPE(self) = &mrb_tinytemplate_data_type;
  DATA_PTR(self) = tt;
  return self;
}

static mrb_value
mrb_tinytemplate_render(mrb_state* mrb, mrb_value self) {
  mrb_tinytemplate* tt = (mrb_tinytemplate*) DATA_PTR(self);
  mrb_value hash = mrb_nil_value();

  mrb_get_args(mrb, "|H", &hash);

  if (mrb_hash_p(hash)) {
    mrb_value keys = mrb_hash_keys(mrb, hash);
    int i, l = mrb_ary_len(mrb, keys);
    for (i = 0; i < l; i++) {
      mrb_value key = mrb_ary_entry(keys, i);
      mrb_value val = mrb_hash_get(mrb, hash, key);
      std::string k = "$";
      k += mrb_str_to_cstr(mrb, key);
      mrb_gv_set(
        tt->mrb,
        mrb_intern(tt->mrb, k.c_str()),
        val);
    }
  }

  mrb_value code = mrb_str_new_cstr(
    tt->mrb,
    tt->tmpl.c_str());

  mrb_value ret = mrb_nil_value();
  mrbc_context* cxt = mrbc_context_new(tt->mrb);
  cxt->capture_errors = 1;
  struct mrb_parser_state *parser = mrb_parser_new(tt->mrb);
  parser->s = RSTRING_PTR(code);
  parser->send = RSTRING_PTR(code) + RSTRING_LEN(code);
  parser->lineno = 1;
  mrb_parser_parse(parser, cxt);
  int n = mrb_generate_code(tt->mrb, parser);
  if (0 < parser->nerr) {
    mrbc_context_free(tt->mrb, cxt);
    mrb_raisef(mrb, E_RUNTIME_ERROR, "line %d: %s\n", parser->error_buffer[0].lineno, parser->error_buffer[0].message);
  } else {
    int ai = mrb_gc_arena_save(tt->mrb);
    mrb_value out = mrb_run(
      tt->mrb,
      mrb_proc_new(tt->mrb, tt->mrb->irep[n]),
      mrb_top_self(tt->mrb));
    mrb_gc_arena_restore(tt->mrb, ai);
    ret = mrb_str_new_cstr(
      mrb,
      mrb_str_to_cstr(
        tt->mrb,
        mrb_funcall(
          tt->mrb,
          tt->mrb->exc ? mrb_obj_value(tt->mrb->exc) : out, "to_s", 0, NULL)));
    mrbc_context_free(tt->mrb, cxt);
    if (tt->mrb->exc) {
      mrb_raisef(mrb, E_RUNTIME_ERROR, mrb_str_to_cstr(mrb, ret));
    }
  }
  return ret;
}

extern "C" {

void
mrb_mruby_tinytemplate_gem_init(mrb_state* mrb) {
  struct RClass* _class_tinytemplate = mrb_define_class(mrb, "TinyTemplate", mrb->object_class);
  MRB_SET_INSTANCE_TT(_class_tinytemplate, MRB_TT_DATA);
  mrb_define_method(mrb, _class_tinytemplate, "initialize", mrb_tinytemplate_init, ARGS_REQ(1));
  mrb_define_method(mrb, _class_tinytemplate, "render", mrb_tinytemplate_render, ARGS_NONE());
}

void
mrb_mruby_tinytemplate_gem_final(mrb_state* mrb) {
}

}
