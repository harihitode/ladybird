#include "trigger.h"
#include "riscv.h"
#include "gdbstub_sys.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void trig_init(trigger_t *trig) {
  trig->size = 0;
  trig->elem = NULL;
  return;
}

unsigned trig_size(const trigger_t *trig) {
  return trig->size;
}

void trig_resize(trigger_t *trig, unsigned size) {
  if (trig->size < size) {
    trig->elem = (struct trigger_elem **)realloc(trig->elem, size * sizeof(struct trigger_elem *));
    for (unsigned i = trig->size; i < size; i++) {
      trig->elem[i] = (struct trigger_elem *)calloc(1, sizeof(struct trigger_elem));
    }
    trig->size = size;
  }
}

unsigned trig_get_tdata(const trigger_t *trig, unsigned index, unsigned no) {
  if (index < trig->size) {
    const struct trigger_elem *elem = trig->elem[index];
    if (no == 0) {
      if (elem->type == CSR_TDATA1_TYPE_MATCH6) {
        return ((elem->type << CSR_TDATA1_TYPE_FIELD) |
                (elem->dmode << CSR_TDATA1_DMODE_FIELD) |
                (elem->vs << 24) |
                (elem->vu << 23) |
                (elem->select << 21) |
                (elem->timing << 20) |
                (elem->action << 12) |
                (elem->m << 6) |
                (elem->s << 4) |
                (elem->u << 3) |
                (elem->access));
      } else if (trig->elem[index]->type == CSR_TDATA1_TYPE_ICOUNT) {
        return ((elem->type << CSR_TDATA1_TYPE_FIELD) |
                (elem->dmode << CSR_TDATA1_DMODE_FIELD) |
                (elem->vs << 26) |
                (elem->vu << 25) |
                ((elem->count & 0x03fff) << 10) |
                (elem->m << 9) |
                (elem->pending << 8) |
                (elem->s << 7) |
                (elem->u << 6) |
                (elem->action));
      } else {
        return 0;
      }
    } else if (no == 1) {
      return trig->elem[index]->data2;
    } else if (no == 2) {
      return trig->elem[index]->data3;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

void trig_set_tdata(trigger_t *trig, unsigned index, unsigned no, unsigned data) {
  if (index < trig->size) {
    struct trigger_elem *elem = trig->elem[index];
    if (no == 0) {
      memset(elem, 0, sizeof(struct trigger_elem));
      elem->type = (data >> 28) & 0x0f;
      elem->dmode = (data >> 27) & 0x01;
      switch (elem->type) {
      case CSR_TDATA1_TYPE_MATCH6:
        elem->vs = (data >> 24) & 0x1;
        elem->vu = (data >> 23) & 0x1;
        elem->select = (data >> 21) & 0x1;
        elem->timing = (data >> 20) & 0x1;
        elem->action = (data >> 12) & 0x0f;
        elem->m = (data >> 6) & 0x1;
        elem->s = (data >> 4) & 0x1;
        elem->u = (data >> 3) & 0x1;
        elem->access = (data & 0x7);
        break;
      case CSR_TDATA1_TYPE_ICOUNT:
        elem->vs = (data >> 26) & 0x1;
        elem->vu = (data >> 25) & 0x1;
        elem->count = (data >> 10) & 0x03fff;
        elem->m = (data >> 9) & 0x1;
        elem->pending = (data >> 8) & 0x1;
        elem->s = (data >> 7) & 0x1;
        elem->u = (data >> 6) & 0x1;
        elem->action = (data & 0x3f);
        break;
      default:
        break;
      }
    } else if (no == 1) {
      elem->data2 = data;
    } else if (no == 2) {
      elem->data3 = data;
    }
  }
}

unsigned trig_info(const trigger_t *trig, unsigned index) {
  if (index < trig->size) {
    // TODO
    return 0x0000ffff;
  } else {
    return 0x00000001;
  }
}

static unsigned trig_match6_fire(struct trigger_elem *elem, const struct dbg_step_result *result) {
  if ((elem->access & result->m_access) && (elem->data2 == result->m_vaddr)) {
    return 1;
  } else {
    return 0;
  }
}

static unsigned trig_icount_fire(struct trigger_elem *elem, const struct dbg_step_result *result) {
  if (elem->count > 1) {
    elem->count--;
    return 0;
  } else if (elem->count == 1) {
    elem->count--;
    elem->pending = 1;
    return 1;
  } else {
    return elem->pending;
  }
}

void trig_cycle(trigger_t *trig, struct dbg_step_result *result) {
  for (unsigned i = 0; i < trig->size; i++) {
    switch (trig->elem[i]->type) {
    case CSR_TDATA1_TYPE_MATCH6:
      result->trigger = trig_match6_fire(trig->elem[i], result);
      break;
    case CSR_TDATA1_TYPE_ICOUNT:
      result->trigger = trig_icount_fire(trig->elem[i], result);
      break;
    default:
      result->trigger = 0;
      break;
    }
    if (result->trigger) {
      break;
    }
  }
  return;
}


void trig_fini(trigger_t *trig) {
  for (unsigned i = 0; i < trig->size; i++) {
    free(trig->elem[i]);
  }
  free(trig->elem);
}
