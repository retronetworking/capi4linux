/*
 *  $Id$
 *
 *  Copyright(C) 2004 Frank A. Uepping
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/isdn/capidevice.h>


#ifdef CONFIG_PROC_FS
extern struct list_head capi_appls_list;
extern struct semaphore capi_appls_list_sem;


static struct capi_appl*
get_capi_appl_by_idx(loff_t idx)
{
	struct capi_appl* a;

	list_for_each_entry(a, &capi_appls_list, entry)
		if (!idx--)
			return a;

	return NULL;
}


static inline struct capi_appl*
get_capi_appl_next(struct capi_appl* a)
{
	struct list_head* n = a->entry.next;

	return n != &capi_appls_list ? list_entry(n, struct capi_appl, entry) : NULL;
}


static void*
appl_start(struct seq_file* seq, loff_t* pos)
{
	down(&capi_appls_list_sem);

	return *pos ? get_capi_appl_by_idx(*pos - 1) : SEQ_START_TOKEN;
}


static void*
appl_next(struct seq_file* seq, void* v, loff_t* pos)
{
	++*pos;

	return v == SEQ_START_TOKEN ? get_capi_appl_by_idx(0) : get_capi_appl_next(v);
}


static void
appl_stop(struct seq_file* seq, void* v)
{
	up(&capi_appls_list_sem);
}


/* -------------------------------------------------------------------------- */


static int
applparams_show(struct seq_file* seq, void* v)
{
	if (v == SEQ_START_TOKEN)
		seq_puts(seq, "ID   : LogConns DataPackets DataLen\n");
	else {
		const struct capi_appl* a = v;

		seq_printf(seq, "%-5u: %-8d %-11d %-7d\n",
			   a->id,
			   a->params.level3cnt,
			   a->params.datablkcnt,
			   a->params.datablklen);
	}

	return 0;
}


static struct seq_operations applparams_seq_ops = {
	.start	= appl_start,
	.next	= appl_next,
	.stop	= appl_stop,
	.show	= applparams_show
};


static int
applparams_open(struct inode* inode, struct file* file)
{
	return seq_open(file, &applparams_seq_ops);
}


static struct file_operations applparams_file_ops = {
	.owner		= THIS_MODULE,
	.open		= applparams_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release
};


/* -------------------------------------------------------------------------- */


static int
applstats_show(struct seq_file* seq, void* v)
{
	if (v == SEQ_START_TOKEN)
		seq_puts(seq, "ID   : txPackets txBytes txDataPackets txDataBytes | rxPackets rxBytes rxDataPackets rxDataBytes rxQueue\n");
	else {
		const struct capi_appl* a = v;

		seq_printf(seq, "%-5u: %-9lu %-7lu %-13lu %-11lu | %-9lu %-7lu %-13lu %-11lu %-7u\n",
			   a->id,
			   a->stats.tx_packets,
			   a->stats.tx_bytes,
			   a->stats.tx_data_packets,
			   a->stats.tx_data_bytes,
			   a->stats.rx_packets,
			   a->stats.rx_bytes,
			   a->stats.rx_data_packets,
			   a->stats.rx_data_bytes,
			   skb_queue_len(&a->msg_queue));
	}

	return 0;
}


static struct seq_operations applstats_seq_ops = {
	.start	= appl_start,
	.next	= appl_next,
	.stop	= appl_stop,
	.show	= applstats_show
};


static int
applstats_open(struct inode* inode, struct file* file)
{
	return seq_open(file, &applstats_seq_ops);
}


static struct file_operations applstats_file_ops = {
	.owner		= THIS_MODULE,
	.open		= applstats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release
};


/* -------------------------------------------------------------------------- */


static struct proc_dir_entry* proc_capi;


static int
create_seq_entry(const char* name, struct file_operations* fops)
{
	struct proc_dir_entry* p = create_proc_entry(name, 0444, proc_capi);
	if (!p)
		return -ENOMEM;

	p->proc_fops = fops;
	p->owner = THIS_MODULE;

	return 0;
}


int __init
capi_register_proc(void)
{
	proc_capi = proc_mkdir("capi", NULL);
	if (!proc_capi)
		return -ENOMEM;
	proc_capi->owner = THIS_MODULE;

	if (create_seq_entry("applparams", &applparams_file_ops))
		goto Err1;

	if (create_seq_entry("applstats", &applstats_file_ops))
		goto Err2;

	return 0;

 Err2:	remove_proc_entry("applparams", proc_capi);
 Err1:	remove_proc_entry("capi", NULL);

	return -ENOMEM;
}


void __exit
capi_unregister_proc(void)
{
	remove_proc_entry("applstats", proc_capi);
	remove_proc_entry("applparams", proc_capi);

	remove_proc_entry("capi", NULL);
}


#else  /* !CONFIG_PROC_FS */
int __init
capi_register_proc(void)
{
	return 0;
}


void __exit
capi_unregister_proc(void)
{
}
#endif  /* CONFIG_PROC_FS */
