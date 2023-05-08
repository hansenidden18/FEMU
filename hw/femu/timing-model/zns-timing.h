#ifndef __FEMU_ZNS_TIMING_MODEL
#define __FEMU_ZNS_TIMING_MODEL

#define ZNS_BLK_BITS    (32)
#define ZNS_FC_BITS     (8)
#define ZNS_CH_BITS     (8)

enum {
    ZNS_READ =  0,
    ZNS_WRITE = 1,
    ZNS_ERASE = 2,
};

typedef struct FemuCtrl FemuCtrl;
typedef struct NvmeNamespace NvmeNamespace;

struct zns_ppa {
    union {
        struct {
	    uint64_t blk : ZNS_BLK_BITS;
	    uint64_t fc  : ZNS_FC_BITS;
	    uint64_t ch  : ZNS_CH_BITS;
	    uint64_t rsv : 1;
        } g;

	uint64_t zns_ppa;
    };
};

struct zns_write_pointer {
    uint64_t ch;
    uint64_t lun;
};

struct zns_nand_cmd {
    int cmd;
    uint64_t stime;
};

struct zns_blk {
    uint64_t next_blk_avail_time;
};

struct zns_fc {
    struct zns_blk *blk;
    uint64_t next_fc_avail_time;
};

struct zns_ch {
    struct zns_fc *fc;
    uint64_t next_ch_avail_time;
};

struct zns_ssd {
    uint64_t num_ch;
    uint64_t num_lun;
    struct zns_ch *ch;
    struct zns_write_pointer wp;
};

void zns_init_blk(struct zns_blk *blk);
void zns_init_fc(struct zns_fc *fc);
void zns_init_ch(struct zns_ch *ch, uint8_t num_lun);
void zns_init_params(FemuCtrl *n);

uint64_t zone_slba(FemuCtrl *n, uint32_t zone_idx);
void zns_check_addr(int a, int max);
void advance_read_pointer(FemuCtrl *n);
struct zns_ppa lpn_to_ppa(FemuCtrl *n, NvmeNamespace *ns, uint64_t lpn);
uint64_t zns_advance_status(FemuCtrl *n, struct zns_nand_cmd *ncmd, struct zns_ppa *ppa);

#endif
