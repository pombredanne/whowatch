#include "whowatch.h"
#include "proctree.h"

//static char *hlp = "\001[ENT]users [c]md [d]etails [o]owner [s]ysinfo sig[l]ist ^[K]ILL";
static char *hlp_init_pid = "\001[ENT]users [d]etails [o]owner [s]ysinfo [^k]ILL [r]edraw";
static char *hlp = "\001[ENT]users all[t]ree [d]etails [o]owner [s]ysinfo [^k]ILL [r]edraw";

static struct process *begin;
static pid_t tree_root = 1;
static unsigned int show_owner;

static void proc_del(struct process *p)
{						
	*p->prev=p->next;				
	if (p->next) p->next->prev = p->prev;
	if (p->proc) p->proc->priv = 0;	
	free(p);
//	proc_win.d_lines--;					
//	allocated--;					
}

static void mark_del(void *vp)
{
	struct process *p = (struct process *) vp;
	struct proc_t *q;
	q = p->proc;
	for(q = q->child; q; q = q->broth.nx)
		if (q->priv) mark_del(q->priv);

	p->proc->priv = 0;	
	p->proc = 0;
}

static inline int is_marked(struct process *p)
{
	return p->proc?0:1;
}

static void clear_list()
{
	struct process *p, *q;
	for(p = begin; p; p = q){
		q = p->next;
		proc_del(p);
	}
}

static void synchronize(struct wdgt *w)
{
	int l = 0;
	struct proc_t *p = tree_start(tree_root, tree_root);
	struct process **current = &begin, *z;
	while(p){
		if (*current && p->priv){
			(*current)->line = l++;
			(*current)->proc->priv = *current;
			p = tree_next();
			current = &((*current)->next);
			continue;
		}
		z = malloc(sizeof *z);
		if (!z) allocate_error();
		//allocated++;
	//	proc_win.d_lines++;
		memset(z, 0, sizeof *z);
		scr_linserted(w, l);
		z->line = l++;
		p->priv = z;
		z->proc = p;
		if (*current){
			z->next = *current;
			(*current)->prev = &z->next;
		}
		*current = z;
		z->prev = current;
		current = &(z->next);
		p = tree_next();
	}

}

static void delete_tree_lines(struct wdgt *w)
{
	struct process *u, *p;
	p = begin;
	while(p){
		if (!is_marked(p)){
			p = p->next;
			continue;
		}
		//delete_line(&proc_win, p->line);
		scr_ldeleted(w, p->line);
		u = p;
		while(u) {
			u->line--;
			u = u->next;
		}
		u = p->next;
		proc_del(p);
		p = u;
	}
}

static char get_state_color(char state)
{
	static char m[]="R DZT?", c[]="\5\2\6\4\7\7";
	char *s = strchr(m, state);
	if (!s) return '\3';
	return c[s - m];	
}

static char *prepare_line(struct wdgt *w, struct process *p)
{
	char *tree;
	if (!p) return 0;
	tree = tree_string(tree_root, p->proc);
	get_state(p);
	if(show_owner) 
		snprintf(w->mwin->gbuf, w->mwin->gbsize ,"\x3%5d %c%c \x3%-8s \x2%s \x3%s", 
			p->proc->pid, get_state_color(p->state), 
			p->state, get_owner_name(proc_pid_uid(p->proc->pid)), tree, 
			get_cmdline(p->proc->pid));
	 else 
		snprintf(w->mwin->gbuf, w->mwin->gbsize,"\x3%5d %c%c \x2%s \x3%s",
			p->proc->pid, get_state_color(p->state), 
			p->state, tree, get_cmdline(p->proc->pid));
		
	return w->mwin->gbuf;
}

/*
 * Find process by its command line. 
 * Processes that have line number (data line, not screen)
 * lower than second argument are skipped. Called by do_search.
 */
unsigned int getprocbyname(int l)
{
	struct process *p;
	char *tmp, buf[8];
	for(p = begin; p ; p = p->next){
		if(!p->proc) continue;
		if(p->line <= l) continue;
		/* try the pid first */
		snprintf(buf, sizeof buf, "%d", p->proc->pid);
		if(reg_match(buf)) return p->line;
		/* next process owner */
		if(show_owner && reg_match(get_owner_name(p->uid))) 
			return p->line;
		tmp = get_cmdline(p->proc->pid);
		if(reg_match(tmp)) return p->line;
	}
	return -1;
}
/*
void tree_title(struct user_t *u)
{
	//char buf[64];
return;	
//	if(!u) snprintf(buf, sizeof buf, "%d processes", proc_win.d_lines);
//	else snprintf(buf, sizeof buf, "%-14.14s %-9.9s %-6.6s %s",
//               	u->parent, u->name, u->tty, u->host);
//	wattrset(info_win.wd, A_BOLD);
  //      echo_line(&info_win, buf, 1);
}
*/

static void draw_tree(struct wdgt *w)
{
	struct process *p;
	/* init scr output function */
	scr_output_start(w);
	if(!begin) {
		scr_maddstr(w, "User has logged out", 0, 0, 20);
		return;
	}
	for(p = begin; p ;p = p->next) {
		if(!p->proc) continue;
		scr_addfstr(w, prepare_line(w, p), p->line, 0);
	}
	scr_output_end(w);
}

static void tree_periodic(void)
{
	update_tree(mark_del);
	//delete_tree_lines();
	//synchronize();
//	draw_tree();
}
void show_tree(pid_t pid)
{
//        print_help(state);
	//proc_win.offset = proc_win.cursor = 0;
	//tree_root = INIT_PID;
//	if(pid > 0) tree_root = pid; 
        tree_periodic();
//        tree_title(pid);
//	tree_title(0); // change it!
}

/*
 * Find pid of the process that is on cursor line
 */
static pid_t crsr_pid(int line)
{
	struct process *p;
	for(p = begin; p; p = p->next){
		if(p->line == line) return p->proc->pid;
	}
	return 0;
}	
static void ptree_periodic(struct wdgt *w)
{
	DBG("**** building process tree root %d *****", tree_root);
	update_tree(mark_del);
	delete_tree_lines(w);
	synchronize(w);
}

static void ptree_redraw(struct wdgt *w)
{
	DBG("--- redraw tree");
	//tree_root = INIT_PID;
	scr_werase(w);
	draw_tree(w);
}

void do_signal(struct wdgt *w, int sig, int pid)
{
	send_signal(sig, pid);
	ptree_periodic(w);
	WNEED_REDRAW(w);
}

static int signal_keys(struct wdgt *w, int key)
{
        int signal = 0;
	
	if(!(key&KBD_CTRL)) return KEY_SKIPPED;
	key &= KBD_MASK;
	switch(key) {
	case 'K': signal = 9; break;
	case 'U': signal = 1; break;
	case 'T': signal = 15; break;
	}
	if(signal) do_signal(w, signal, crsr_pid(w->crsr));
	return KEY_HANDLED;
}	

static void *pmsgh(struct wdgt *w, int type, struct wdgt *s, void *d)
{
	static u32 pid;
	switch(type) {
		case MWANT_DSOURCE: return eproc;
		case MWANT_CRSR_VAL: pid = crsr_pid(w->crsr); return &pid;
		case MALL_CRSR_REG: w->flags |= WDGT_CRSR_SND; 
				    DBG("%s wants every cursor change", s->name);break;
		case MALL_CRSR_UNREG: w->flags &= ~ WDGT_CRSR_SND; break;
	}
	return 0;
}

static void ptree_hlp(struct wdgt *w)
{
	char *s = hlp_init_pid;
	if(tree_root != INIT_PID) s = hlp;
	wmsg_send(w, MCUR_HLP, s);
}

static void ptree_unhide(struct wdgt *w)
{
	ptree_hlp(w);
	ptree_periodic(w);
	w->periodic = ptree_periodic;
	w->msgh = pmsgh;
	w->redraw = ptree_redraw;
	w->wrefresh = scr_wrefresh;
	WNEED_REDRAW(w);	
	w->vy = w->vy = w->crsr = 0;
}	

static void ptree_hide(struct wdgt *w)
{
	DBG("Deleting ptree  from lists");
	w->redraw = w->wrefresh = 0;
	clear_list();
	w->periodic = 0;
	w->msgh = 0;
}	

static void root_change(struct wdgt *w)
{
	void *p;
	p = wmsg_send(w, MWANT_UPID, 0);
	if(p) tree_root = *(pid_t*)p;
	else tree_root = INIT_PID;
}

/*
 * Give up main window if currently printing output there
 * or start printing if not there
 */
static void pswitch(struct wdgt *w, int k, int p)
{
        if(!WIN_HIDDEN(w)) {
		if(k == p || k == KBD_ENTER) {
			if(k != 't') ptree_hide(w); 
			return;
		}		
		if(k == 't') {
			tree_root = INIT_PID;
			ptree_periodic(w);
			WNEED_REDRAW(w);
			return;
		}
	} else {
		if(k == KBD_ENTER) root_change(w);
		else if(k == 't') tree_root = INIT_PID;
		wmsg_send(w, MSND_ESOURCE, eproc);
		ptree_unhide(w);
	}	
}

static void crsr_send(struct wdgt *w)
{
	static u32 pid;
	
	if(!(w->flags & WDGT_CRSR_SND)) return;
	if(!(pid = crsr_pid(w->crsr))) return;
	DBG("sending current pid %d", pid);
	wmsg_send(w, MCUR_CRSR, &pid);
}

static int pkeyh(struct wdgt *w, int key)
{
        int ret = KEY_HANDLED;
	int ctrl = key&KBD_MASK;
	static int pkey;
	
	/* hide or unhide wdgt */
	if(key == 't' || key == KBD_ENTER) { 
		pswitch(w, key, pkey);
		pkey = key;
		return KEY_HANDLED;
	}
	/* must be visible to process keys below */
	if(WIN_HIDDEN(w)) return KEY_SKIPPED;
	if(ctrl == 'K') {
		signal_keys(w, key);
		return ret;
	}
        switch(key) {
		case 'o': show_owner ^= 1; WNEED_REDRAW(w); break;
		case 'r': ptree_periodic(w); WNEED_REDRAW(w); break;
        /*        case KBD_UP:
		case KBD_DOWN:
		case KBD_PAGE_UP:
		case KBD_PAGE_DOWN: */
		default:	 ret = scr_keyh(w, key);
			 if(ret == KEY_HANDLED) crsr_send(w);
	}
        return ret;
}

/*
 * Register all process tree functions here.
 */ 
void ptree_reg(struct wdgt *w)
{
	w->keyh     = pkeyh;
	w->msgh	    = pmsgh;
	mwin_msg_on(w);
}

