#ifndef TRIGGER_H
#define TRIGGER_H

struct dbg_step_result;

struct trigger_elem {
  unsigned char type;
  unsigned char dmode;
  unsigned char vs;
  unsigned char vu;
  unsigned char m;
  unsigned char s;
  unsigned char u;
  unsigned char action;
  unsigned char timing;
  unsigned char select;
  unsigned char access;
  unsigned count; // 14bit counter for icount
  unsigned pending;
  unsigned data2;
  unsigned data3;
};

typedef struct trigger_t {
  unsigned size;
  struct trigger_elem **elem;
} trigger_t;

void trig_init(trigger_t *trig);
unsigned trig_size(const trigger_t *trig);
void trig_resize(trigger_t *trig, unsigned size);
unsigned trig_get_tdata(const trigger_t *trig, unsigned index, unsigned no);
void trig_set_tdata(trigger_t *trig, unsigned index, unsigned no, unsigned data);
unsigned trig_info(const trigger_t *trig, unsigned index);
void trig_cycle(trigger_t *trig, struct dbg_step_result *result);
void trig_fini(trigger_t *trig);

#endif
