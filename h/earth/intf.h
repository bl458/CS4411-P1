#include <earth/log.h>
#include <earth/intr.h>
#include <earth/tlb.h>
#include <earth/clock.h>
#include <earth/devdisk.h>
#include <earth/devgate.h>
#include <earth/devtty.h>
#include <earth/devudp.h>

/* Interface to earth.
 */
struct earth_intf {
	struct log_intf log;
	struct intr_intf intr;
	struct tlb_intf tlb;
	struct clock_intf clock;

	struct dev_disk_intf dev_disk;
	struct dev_gate_intf dev_gate;
	struct dev_tty_intf dev_tty;
	struct dev_udp_intf dev_udp;
};

extern struct earth_intf earth;

void earth_setup();
