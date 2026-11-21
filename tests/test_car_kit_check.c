/* dep85 step 3: the wheel rules of a car kit's check (formats/car_kit_check.h). A good car gives no problem; each slip gives its own id, with the
 * object's name in the message and the right severity; a Blender number after a dot is the same wheel, twice is an error. */
#include "formats/car_kit_check.h"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *data; size_t len, cap; } Buf;

static void put(Buf *b, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void put(Buf *b, const char *fmt, ...)
{
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (b->len + (size_t)n + 1 > b->cap) {
        b->cap = (b->len + (size_t)n + 1) * 2;
        b->data = realloc(b->data, b->cap);
    }
    memcpy(b->data + b->len, line, (size_t)n + 1);
    b->len += (size_t)n;
}

static void box(Buf *b, int *v, double x0, double x1, double y0, double y1, double z0, double z1, int faces)
{
    const double xs[2] = { x0, x1 }, ys[2] = { y0, y1 }, zs[2] = { z0, z1 };
    for (int i = 0; i < 8; i++) put(b, "v %g %g %g\n", xs[i & 1], ys[(i >> 1) & 1], zs[(i >> 2) & 1]);
    if (faces) {
        const int o = *v;
        put(b, "f %d %d %d %d\nf %d %d %d %d\n", o, o + 1, o + 3, o + 2, o + 4, o + 6, o + 7, o + 5);
    }
    *v += 8;
}

/* a wheel as a box: x from x0 to x1, its bottom at y0, `d` high and deep, centred on z */
static void wheel(Buf *b, int *v, const char *name, double x0, double x1, double y0, double h, double depth, double z, int faces)
{
    put(b, "o %s\n", name);
    box(b, v, x0, x1, y0, y0 + h, z - depth / 2, z + depth / 2, faces);
}

typedef struct { const char *fl, *fr, *rl, *rr; double fl_w, rl_depth, fr_y, rr_h, fl_z; int fl_faces; } Car;

static ObjModel read_car(const Car *c, int extra)
{
    Buf b = { NULL, 0, 0 };
    int v = 1;
    put(&b, "o body\n");
    box(&b, &v, -100, 100, 20, 130, -260, 260, 1);
    if (c->fl) wheel(&b, &v, c->fl, -120, -120 + c->fl_w, 0, 65, 65, c->fl_z, c->fl_faces);
    if (c->fr) wheel(&b, &v, c->fr, 90, 120, c->fr_y, 65, 65, -170, 1);
    if (c->rl) wheel(&b, &v, c->rl, -120, -90, 0, 65, c->rl_depth, 170, 1);
    if (c->rr) wheel(&b, &v, c->rr, 90, 120, 0, c->rr_h, c->rr_h, 170, 1);
    if (extra) wheel(&b, &v, "wheel_fl.001", -120, -90, 0, 65, 65, -100, 1);
    ObjModel m;
    ExchangeError err;
    memset(&err, 0, sizeof err);
    CHECK(obj_read((const uint8_t *)b.data, b.len, NULL, 0, &m, &err));
    free(b.data);
    return m;
}

static Car good(void)
{
    return (Car){ "wheel_fl", "Wheel_FR", "wheel_rl", "wheel_rr", 30, 65, 0, 65, -170, 1 };
}

static const KitProblem *find(const KitProblems *p, const char *id)
{
    for (size_t i = 0; i < p->count; i++)
        if (strcmp(p->item[i].id, id) == 0) return &p->item[i];
    return NULL;
}

static size_t run(const Car *c, int extra, KitProblems *p)
{
    ObjModel m = read_car(c, extra);
    kit_problems_init(p);
    CHECK(car_kit_check_wheels(&m, p));
    obj_free(&m);
    return p->count;
}

static void expect(const char *what, Car c, int extra, const char *id, KitSeverity sev, const char *in_text, size_t count)
{
    KitProblems p;
    run(&c, extra, &p);
    const KitProblem *f = find(&p, id);
    if (f == NULL) fprintf(stderr, "no %s for %s\n", id, what);
    CHECK(f != NULL && f->severity == sev && (in_text == NULL || strstr(f->text, in_text) != NULL));
    CHECK(p.count == count);   /* the slip gives its rule, not a crowd */
    kit_problems_free(&p);
}

int main(void)
{
    {   /* a good car: nothing, and the case of the name does not matter */
        Car c = good();
        KitProblems p;
        CHECK_EQ_INT(run(&c, 0, &p), 0);
        CHECK_EQ_INT(kit_problems_errors(&p), 0);
        kit_problems_free(&p);
    }
    {   Car c = good(); c.fl = NULL; expect("no front wheel", c, 0, "wheel-missing", KIT_ERROR, "wheel_fl", 1); }
    {   Car c = good(); c.fr = NULL; expect("no front wheel", c, 0, "wheel-missing", KIT_ERROR, "wheel_fr", 1); }
    {   Car c = good(); c.rl = NULL; expect("no rear wheel", c, 0, "wheel-missing-rear", KIT_WARNING, "wheel_rl", 1); }
    {   Car c = good(); expect("a second wheel_fl", c, 1, "wheel-twice", KIT_ERROR, "wheel_fl.001", 1); }
    {   Car c = good(); c.fl_faces = 0; expect("a wheel with no face", c, 0, "wheel-no-faces", KIT_ERROR, "wheel_fl", 1); }
    {   Car c = good(); c.rl_depth = 40; expect("a rear wheel flat", c, 0, "wheel-not-round", KIT_WARNING, "wheel_rl", 1); }
    {   Car c = good(); c.fl_w = 100; expect("a wide wheel", c, 0, "wheel-too-wide", KIT_WARNING, "wheel_fl", 1); }
    {   Car c = good(); c.fl = "wheel_fr"; c.fr = "wheel_fl"; expect("sides swapped", c, 0, "wheel-side", KIT_WARNING, NULL, 2); }
    {   Car c = good(); c.fl_z = 400; expect("a front wheel behind", c, 0, "wheel-order", KIT_WARNING, "wheel_fl", 1); }
    {   Car c = good(); c.fr_y = 40; expect("one wheel up", c, 0, "wheel-ground", KIT_WARNING, NULL, 1); }
    {   Car c = good(); c.rr_h = 40; expect("a smaller wheel", c, 0, "wheel-pair-size", KIT_WARNING, "wheel_rl", 1); }
    return TEST_REPORT();
}
