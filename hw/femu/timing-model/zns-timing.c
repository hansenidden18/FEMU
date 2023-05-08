#include "../nvme.h"

void zns_init_blk(struct zns_blk *blk)
{
    blk->next_blk_avail_time = 0;
}

void zns_init_fc(struct zns_fc *fc)
{
    fc->blk = g_malloc0(sizeof(struct zns_blk) * 32);
    for (int i = 0; i < 32; i++) {
        zns_init_blk(&fc->blk[i]);
    }
    fc->next_fc_avail_time = 0;
}

void zns_init_ch(struct zns_ch *ch, uint8_t num_lun)
{
    ch->fc = g_malloc0(sizeof(struct zns_fc) * num_lun);
    for (int i = 0; i < num_lun; i++) {
        zns_init_fc(&ch->fc[i]);
    }
    ch->next_ch_avail_time = 0;
}

void zns_init_params(FemuCtrl *n)
{
    struct zns_ssd *id_zns;
    int i;

    id_zns = g_malloc0(sizeof(struct zns_ssd));
    id_zns->num_ch = n->zns_params.zns_num_ch;
    id_zns->num_lun = n->zns_params.zns_num_lun;
    id_zns->ch = g_malloc0(sizeof(struct zns_ch) * id_zns->num_ch);
    for (i =0; i < id_zns->num_ch; i++) {
        zns_init_ch(&id_zns->ch[i], id_zns->num_lun);
    }

    id_zns->wp.ch = 0;
    id_zns->wp.lun = 0;
    n->zns = id_zns;
}

inline struct zns_ch *get_ch(struct zns_ssd *zns, struct zns_ppa *ppa)
{
    return &(zns->ch[ppa->g.ch]);
}

inline struct zns_fc *get_fc(struct zns_ssd *zns, struct zns_ppa *ppa)
{
    struct zns_ch *ch = get_ch(zns, ppa);
    return &(ch->fc[ppa->g.fc]);
}

inline struct zns_blk *get_blk(struct zns_ssd *zns, struct zns_ppa *ppa)
{
    struct zns_fc *fc = get_fc(zns, ppa);
    return &(fc->blk[ppa->g.blk]);
}

inline uint32_t zns_zone(NvmeNamespace *ns, uint64_t slba)
{
    FemuCtrl *n = ns->ctrl;

    return (n->zone_size_log2 > 0 ? slba >> n->zone_size_log2 : slba / n->zone_size);
}

inline uint64_t zone_slba(FemuCtrl *n, uint32_t zone_idx)
{
    return (zone_idx) * n->zone_size;
}

void zns_check_addr(int a, int max)
{
   assert(a >= 0 && a < max);
}

void advance_read_pointer(FemuCtrl *n)
{
    struct zns_ssd *zns = n->zns;
    struct zns_write_pointer *wpp = &zns->wp;
    uint8_t num_ch = zns->num_ch;
    uint8_t num_lun = zns->num_lun;

    //printf("NUM CH: %"PRIu64"\n", wpp->ch);
    zns_check_addr(wpp->ch, num_ch);
    wpp->ch++;

    if (wpp->ch != num_ch) {
        return;
    }

    /* Wrap-up, wpp->ch == num_ch */
    wpp->ch = 0;
    zns_check_addr(wpp->lun, num_lun);
    wpp->lun++;
    if (wpp->lun == num_lun) {
        wpp->lun = 0;
        assert(wpp->ch == 0);
        assert(wpp->lun == 0);
    }
}

inline struct zns_ppa lpn_to_ppa(FemuCtrl *n, NvmeNamespace *ns, uint64_t lpn)
{

	uint32_t zone_idx = zns_zone(ns, (lpn * 4096));

	struct zns_ssd *zns = n->zns;
	struct zns_write_pointer *wpp = &zns->wp;
	//uint64_t num_ch = zns->num_ch;
	//uint64_t num_lun = zns->num_lun;
	struct zns_ppa ppa = {0};

	//printf("OFFSET: %"PRIu64"\n\n", offset);
	//wpp->ch,lun
	ppa.g.ch = wpp->ch;
	ppa.g.fc = wpp->lun;
	ppa.g.blk = zone_idx;

    return ppa;
}

uint64_t zns_advance_status(FemuCtrl *n, struct zns_nand_cmd *ncmd, struct zns_ppa *ppa)
{
    int c = ncmd->cmd;

    struct zns_ssd *zns = n->zns;
    uint64_t nand_stime;
    uint64_t req_stime = (ncmd->stime == 0) ? \
        qemu_clock_get_ns(QEMU_CLOCK_REALTIME) : ncmd->stime;

    struct zns_fc *fc = get_fc(zns, ppa);

    uint64_t lat = 0;
    uint64_t read_delay = n->zns_params.zns_read;
    uint64_t write_delay = n->zns_params.zns_write;
    uint64_t erase_delay = 2000000;

    switch (c) {
    case ZNS_READ:
        nand_stime = (fc->next_fc_avail_time < req_stime) ? req_stime : \
                     fc->next_fc_avail_time;
        fc->next_fc_avail_time = nand_stime + read_delay;
        lat = fc->next_fc_avail_time - req_stime;
	    break;

    case ZNS_WRITE:
	    nand_stime = (fc->next_fc_avail_time < req_stime) ? req_stime : \
		            fc->next_fc_avail_time;
	    fc->next_fc_avail_time = nand_stime + write_delay;
	    lat = fc->next_fc_avail_time - req_stime;
	    break;

    case ZNS_ERASE:
        nand_stime = (fc->next_fc_avail_time < req_stime) ? req_stime : \
                        fc->next_fc_avail_time;
        fc->next_fc_avail_time = nand_stime + erase_delay;
        lat = fc->next_fc_avail_time - req_stime;
        break;

    default:
        /* To silent warnings */
        ;
    }

    return lat;
}

