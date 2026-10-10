/* BTree byte-pair coding (codec 0x46). Mirrors NFS_WIN.EXE 004a96f4.
 * After the size: clue byte, node count N, N x (node, left, right), data.
 * In the data a plain byte is emitted, a node byte expands to its pair
 * (recursively), and clue+X emits X literally, or ends the stream if X == 0. */
#include "formats/qfs_codecs.h"

enum {
    KIND_LITERAL = 0,
    KIND_CLUE = 1,
    KIND_NODE = 0xFF,
    MAX_DEPTH = 256  /* a tree over 256 byte values cannot be deeper */
};

typedef struct {
    uint8_t kind[256];
    uint8_t left[256];
    uint8_t right[256];
    uint8_t *dst;
    size_t dst_len;
    size_t out;
} Tree;

static QfsResult emit(Tree *t, uint8_t value)
{
    if (t->out >= t->dst_len) return QFS_ERR_OVERFLOW;
    t->dst[t->out++] = value;
    return QFS_OK;
}

static QfsResult expand(Tree *t, uint8_t node, int depth)
{
    if (depth > MAX_DEPTH) return QFS_ERR_CORRUPT;
    const uint8_t children[2] = { t->left[node], t->right[node] };
    for (int i = 0; i < 2; i++) {
        QfsResult r = t->kind[children[i]] == KIND_NODE ? expand(t, children[i], depth + 1)
                                                        : emit(t, children[i]);
        if (r != QFS_OK) return r;
    }
    return QFS_OK;
}

QfsResult qfs_btree_decode(const uint8_t *src, size_t src_len, size_t data_start, uint8_t *dst, size_t dst_len)
{
    static const Tree empty;
    Tree t = empty;
    t.dst = dst;
    t.dst_len = dst_len;

    size_t in = data_start;
    if (src_len - in < 2) return QFS_ERR_TRUNCATED;
    const uint8_t clue = src[in];
    const size_t nodes = src[in + 1];
    in += 2;
    if ((src_len - in) / 3 < nodes) return QFS_ERR_TRUNCATED;

    /* Same order as the game: the clue is marked first, nodes may override it. */
    t.kind[clue] = KIND_CLUE;
    for (size_t i = 0; i < nodes; i++, in += 3) {
        const uint8_t node = src[in];
        t.left[node] = src[in + 1];
        t.right[node] = src[in + 2];
        t.kind[node] = KIND_NODE;
    }

    for (;;) {
        if (in >= src_len) return QFS_ERR_TRUNCATED;
        const uint8_t c = src[in++];
        QfsResult r;
        if (t.kind[c] == KIND_LITERAL) {
            r = emit(&t, c);
        } else if (t.kind[c] == KIND_NODE) {
            r = expand(&t, c, 0);
        } else {
            if (in >= src_len) return QFS_ERR_TRUNCATED;
            const uint8_t escaped = src[in++];
            if (escaped == 0) break;
            r = emit(&t, escaped);
        }
        if (r != QFS_OK) return r;
    }
    return t.out == dst_len ? QFS_OK : QFS_ERR_SIZE;
}
