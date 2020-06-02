// ---
// title: "Aio"
// date: 2020-05-16T16:12:18+08:00
// description: ""
// draft: true
// tags: []
// categories: []
// ---



// https://github.com/torvalds/linux/commit/2b188cc1bb857a9d4701ae59aa7768b5124e262e#diff-a196e54ec8b5398427f9df3d2b074478



/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Header file for the io_uring interface.
 *
 * Copyright (C) 2019 Jens Axboe
 * Copyright (C) 2019 Christoph Hellwig
 */
#ifndef LINUX_IO_URING_H
#define LINUX_IO_URING_H

#include <linux/fs.h>
#include <linux/types.h>

/*
 * IO submission data structure (Submission Queue Entry)
 */
struct io_uring_sqe {
	__u8	opcode;		/* type of operation for this sqe */  	// read or write
	__u8	flags;		/* as of now unused */
	__u16	ioprio;		/* ioprio for the request */			// 优先级
	__s32	fd;		/* file descriptor to do IO on */			// 指向的文件描述符
	__u64	off;		/* offset into file */					// 文件偏移地址
	__u64	addr;		/* pointer to buffer or iovecs */		// 数据读取或存放的地址 
	__u32	len;		/* buffer size or number of iovecs */	//	
	union {
		__kernel_rwf_t	rw_flags;
		__u32		__resv;
	};
	__u64	user_data;	/* data to be passed back at completion time */
	__u64	__pad2[3];
};

#define IORING_OP_NOP		0
#define IORING_OP_READV		1
#define IORING_OP_WRITEV	2

/*
 * IO completion data structure (Completion Queue Entry)
 */
struct io_uring_cqe {
	__u64	user_data;	/* sqe->data submission passed back */
	__s32	res;		/* result code for this event */
	__u32	flags;
};

/*
 * Magic offsets for the application to mmap the data it needs
 */
#define IORING_OFF_SQ_RING		0ULL
#define IORING_OFF_CQ_RING		0x8000000ULL
#define IORING_OFF_SQES			0x10000000ULL

/*
 * Filled with the offset for mmap(2)
 */
struct io_sqring_offsets {
	__u32 head;
	__u32 tail;
	__u32 ring_mask;
	__u32 ring_entries;
	__u32 flags;
	__u32 dropped;
	__u32 array;
	__u32 resv1;
	__u64 resv2;
};

struct io_cqring_offsets {
	__u32 head;
	__u32 tail;
	__u32 ring_mask;
	__u32 ring_entries;
	__u32 overflow;
	__u32 cqes;
	__u64 resv[2];
};

/*
 * io_uring_enter(2) flags
 */
#define IORING_ENTER_GETEVENTS	(1U << 0)

/*
 * Passed in for io_uring_setup(2). Copied back with updated info on success
 */
struct io_uring_params {
	__u32 sq_entries;				// sqe的个数，得是2的幂次方
	__u32 cq_entries;				// cqe的个数，是sqe的两倍
	__u32 flags;
	__u32 resv[7];
	struct io_sqring_offsets sq_off;
	struct io_cqring_offsets cq_off;
};

#endif


#define IORING_MAX_ENTRIES	4096

struct io_uring {
	u32 head ____cacheline_aligned_in_smp;	// 头所在的索引位置
	u32 tail ____cacheline_aligned_in_smp;	// 尾所在的索引位置
	// 当head == tail时，说明队列为空
};
// io提交队列，
struct io_sq_ring {
	struct io_uring		r;
	u32			ring_mask;		// 掩码，同io_ring_ctx中的掩码
	u32			ring_entries;	// 队列容量，同io_ring_ctx中的队列容量
	u32			dropped;
	u32			flags;
	u32			array[];	// 存放sqe的索引位置（sqe有自己的内存空间，因为次序未定）
};
// io完成队列
struct io_cq_ring {
	struct io_uring		r;
	u32			ring_mask;
	u32			ring_entries;
	u32			overflow;
	struct io_uring_cqe	cqes[];
};

struct io_ring_ctx {
	struct {
		struct percpu_ref	refs;
	} ____cacheline_aligned_in_smp;

	struct {
		unsigned int		flags;
		bool			compat;
		bool			account_mem;

		/* SQ ring */
		struct io_sq_ring	*sq_ring;		// io提交队列
		unsigned		cached_sq_head;
		unsigned		sq_entries;			// 队列项个数
		unsigned		sq_mask;			// 掩码，便于循环
		struct io_uring_sqe	*sq_sqes;		// sqe的内存空间
	} ____cacheline_aligned_in_smp;

	/* IO offload */
	struct workqueue_struct	*sqo_wq;		// 该任务是完成io提交队列中的任务，个数是cpu个数的2倍
	struct mm_struct	*sqo_mm;

	struct {
		/* CQ ring */
		struct io_cq_ring	*cq_ring;		// io完成队列
		unsigned		cached_cq_tail;
		unsigned		cq_entries;			// 队列项个数
		unsigned		cq_mask;			// 掩码，便于循环
		struct wait_queue_head	cq_wait;
		struct fasync_struct	*cq_fasync;
	} ____cacheline_aligned_in_smp;

	struct user_struct	*user;

	struct completion	ctx_done;

	struct {
		struct mutex		uring_lock;
		wait_queue_head_t	wait;
	} ____cacheline_aligned_in_smp;

	struct {
		spinlock_t		completion_lock;
	} ____cacheline_aligned_in_smp;

#if defined(CONFIG_UNIX)
	struct socket		*ring_sock;
#endif
};

struct sqe_submit {
	const struct io_uring_sqe	*sqe;
	unsigned short			index;
	bool				has_user;
};

struct io_kiocb {
	struct kiocb		rw;

	struct sqe_submit	submit;

	struct io_ring_ctx	*ctx;
	struct list_head	list;
	unsigned int		flags;
#define REQ_F_FORCE_NONBLOCK	1	/* inline submission attempt */
	u64			user_data;

	struct work_struct	work;
};

#define IO_PLUG_THRESHOLD		2

static struct kmem_cache *req_cachep;

static const struct file_operations io_uring_fops;

struct sock *io_uring_get_socket(struct file *file)
{
#if defined(CONFIG_UNIX)
	if (file->f_op == &io_uring_fops) {
		struct io_ring_ctx *ctx = file->private_data;

		return ctx->ring_sock->sk;
	}
#endif
	return NULL;
}
EXPORT_SYMBOL(io_uring_get_socket);

static void io_ring_ctx_ref_free(struct percpu_ref *ref)
{
	struct io_ring_ctx *ctx = container_of(ref, struct io_ring_ctx, refs);

	complete(&ctx->ctx_done);
}

static struct io_ring_ctx *io_ring_ctx_alloc(struct io_uring_params *p)
{
	struct io_ring_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	ctx->flags = p->flags;
    // 初始化等待队列，事件是：cq
	init_waitqueue_head(&ctx->cq_wait);
    // 
	init_completion(&ctx->ctx_done);
	mutex_init(&ctx->uring_lock);
	init_waitqueue_head(&ctx->wait);
	spin_lock_init(&ctx->completion_lock);
	return ctx;
}

static void io_commit_cqring(struct io_ring_ctx *ctx)
{
	struct io_cq_ring *ring = ctx->cq_ring;

	if (ctx->cached_cq_tail != READ_ONCE(ring->r.tail)) {
		/* order cqe stores with ring update */
		smp_store_release(&ring->r.tail, ctx->cached_cq_tail);

		/*
		 * Write sider barrier of tail update, app has read side. See
		 * comment at the top of this file.
		 */
		smp_wmb();

		if (wq_has_sleeper(&ctx->cq_wait)) {
			wake_up_interruptible(&ctx->cq_wait);
			kill_fasync(&ctx->cq_fasync, SIGIO, POLL_IN);
		}
	}
}

static struct io_uring_cqe *io_get_cqring(struct io_ring_ctx *ctx)
{
	struct io_cq_ring *ring = ctx->cq_ring;
	unsigned tail;

	tail = ctx->cached_cq_tail;
	/* See comment at the top of the file */
	smp_rmb();
	if (tail + 1 == READ_ONCE(ring->r.head))
		return NULL;

	ctx->cached_cq_tail++;
	return &ring->cqes[tail & ctx->cq_mask];
}

static void io_cqring_fill_event(struct io_ring_ctx *ctx, u64 ki_user_data,
				 long res, unsigned ev_flags)
{
	struct io_uring_cqe *cqe;

	/*
	 * If we can't get a cq entry, userspace overflowed the
	 * submission (by quite a lot). Increment the overflow count in
	 * the ring.
	 */
	cqe = io_get_cqring(ctx);
	if (cqe) {
		WRITE_ONCE(cqe->user_data, ki_user_data);
		WRITE_ONCE(cqe->res, res);
		WRITE_ONCE(cqe->flags, ev_flags);
	} else {
		unsigned overflow = READ_ONCE(ctx->cq_ring->overflow);

		WRITE_ONCE(ctx->cq_ring->overflow, overflow + 1);
	}
}

static void io_cqring_add_event(struct io_ring_ctx *ctx, u64 ki_user_data,
				long res, unsigned ev_flags)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->completion_lock, flags);
	io_cqring_fill_event(ctx, ki_user_data, res, ev_flags);
	io_commit_cqring(ctx);
	spin_unlock_irqrestore(&ctx->completion_lock, flags);

	if (waitqueue_active(&ctx->wait))
		wake_up(&ctx->wait);
}

static void io_ring_drop_ctx_refs(struct io_ring_ctx *ctx, unsigned refs)
{
	percpu_ref_put_many(&ctx->refs, refs);

	if (waitqueue_active(&ctx->wait))
		wake_up(&ctx->wait);
}

static struct io_kiocb *io_get_req(struct io_ring_ctx *ctx)
{
	struct io_kiocb *req;
	req = kmem_cache_alloc(req_cachep, __GFP_NOWARN);
	if (req) {
		req->ctx = ctx;
		req->flags = 0;
		return req;
	}
}

static void io_free_req(struct io_kiocb *req)
{
	io_ring_drop_ctx_refs(req->ctx, 1);
	kmem_cache_free(req_cachep, req);
}

static void kiocb_end_write(struct kiocb *kiocb)
{
	if (kiocb->ki_flags & IOCB_WRITE) {
		struct inode *inode = file_inode(kiocb->ki_filp);

		/*
		 * Tell lockdep we inherited freeze protection from submission
		 * thread.
		 */
		if (S_ISREG(inode->i_mode))
			__sb_writers_acquired(inode->i_sb, SB_FREEZE_WRITE);
		file_end_write(kiocb->ki_filp);
	}
}

static void io_complete_rw(struct kiocb *kiocb, long res, long res2)
{
	struct io_kiocb *req = container_of(kiocb, struct io_kiocb, rw);

	kiocb_end_write(kiocb);

	fput(kiocb->ki_filp);
	io_cqring_add_event(req->ctx, req->user_data, res, 0);
	io_free_req(req);
}

/*
 * If we tracked the file through the SCM inflight mechanism, we could support
 * any file. For now, just ensure that anything potentially problematic is done
 * inline.
 */
static bool io_file_supports_async(struct file *file)
{
	umode_t mode = file_inode(file)->i_mode;

	if (S_ISBLK(mode) || S_ISCHR(mode))
		return true;
	if (S_ISREG(mode) && file->f_op != &io_uring_fops)
		return true;

	return false;
}

static int io_prep_rw(struct io_kiocb *req, const struct io_uring_sqe *sqe,
		      bool force_nonblock)
{
	struct kiocb *kiocb = &req->rw;
	unsigned ioprio;
	int fd, ret;

	fd = READ_ONCE(sqe->fd);
	kiocb->ki_filp = fget(fd);
	kiocb->ki_pos = READ_ONCE(sqe->off);
	kiocb->ki_flags = iocb_flags(kiocb->ki_filp);
	kiocb->ki_hint = ki_hint_validate(file_write_hint(kiocb->ki_filp));

	ioprio = READ_ONCE(sqe->ioprio);
	if (ioprio) {
		ret = ioprio_check_cap(ioprio);
		if (ret)
			goto out_fput;

		kiocb->ki_ioprio = ioprio;
	} else
		kiocb->ki_ioprio = get_current_ioprio();

	ret = kiocb_set_rw_flags(kiocb, READ_ONCE(sqe->rw_flags));
	if (force_nonblock) {
		kiocb->ki_flags |= IOCB_NOWAIT;
		req->flags |= REQ_F_FORCE_NONBLOCK;
	}
	if (kiocb->ki_flags & IOCB_HIPRI) {
		ret = -EINVAL;
		goto out_fput;
	}
	kiocb->ki_complete = io_complete_rw;
	return 0;
out_fput:
	fput(kiocb->ki_filp);
	return ret;
}

static inline void io_rw_done(struct kiocb *kiocb, ssize_t ret)
{
	switch (ret) {
	case -EIOCBQUEUED:
		break;
	case -ERESTARTSYS:
	case -ERESTARTNOINTR:
	case -ERESTARTNOHAND:
	case -ERESTART_RESTARTBLOCK:
		/*
		 * We can't just restart the syscall, since previously
		 * submitted sqes may already be in progress. Just fail this
		 * IO with EINTR.
		 */
		ret = -EINTR;
		/* fall through */
	default:
		kiocb->ki_complete(kiocb, ret, 0);
	}
}

static int io_import_iovec(struct io_ring_ctx *ctx, int rw,
			   const struct sqe_submit *s, struct iovec **iovec,
			   struct iov_iter *iter)
{
	const struct io_uring_sqe *sqe = s->sqe;
	void __user *buf = u64_to_user_ptr(READ_ONCE(sqe->addr));
	size_t sqe_len = READ_ONCE(sqe->len);
	return import_iovec(rw, buf, sqe_len, UIO_FASTIOV, iovec, iter);
}

static ssize_t io_read(struct io_kiocb *req, const struct sqe_submit *s,
		       bool force_nonblock)
{
	struct iovec inline_vecs[UIO_FASTIOV], *iovec = inline_vecs;
	struct kiocb *kiocb = &req->rw;
	struct iov_iter iter;
	struct file *file;
	ssize_t ret;

	ret = io_prep_rw(req, s->sqe, force_nonblock);
	if (ret)
		return ret;
	file = kiocb->ki_filp;

	ret = -EBADF;
	if (unlikely(!(file->f_mode & FMODE_READ)))
		goto out_fput;
	ret = -EINVAL;
	if (unlikely(!file->f_op->read_iter))
		goto out_fput;

	ret = io_import_iovec(req->ctx, READ, s, &iovec, &iter);
	if (ret)
		goto out_fput;

	ret = rw_verify_area(READ, file, &kiocb->ki_pos, iov_iter_count(&iter));
	if (!ret) {
		ssize_t ret2;

		/* Catch -EAGAIN return for forced non-blocking submission */
		ret2 = call_read_iter(file, kiocb, &iter);
		if (!force_nonblock || ret2 != -EAGAIN)
			io_rw_done(kiocb, ret2);
		else
			ret = -EAGAIN;
	}
	kfree(iovec);
out_fput:
	/* Hold on to the file for -EAGAIN */
	if (unlikely(ret && ret != -EAGAIN))
		fput(file);
	return ret;
}

static ssize_t io_write(struct io_kiocb *req, const struct sqe_submit *s,
			bool force_nonblock)
{
	struct iovec inline_vecs[UIO_FASTIOV], *iovec = inline_vecs;
	struct kiocb *kiocb = &req->rw;
	struct iov_iter iter;
	struct file *file;
	ssize_t ret;

	ret = io_prep_rw(req, s->sqe, force_nonblock);
	file = kiocb->ki_filp;
	ret = io_import_iovec(req->ctx, WRITE, s, &iovec, &iter);
	ret = rw_verify_area(WRITE, file, &kiocb->ki_pos,
				iov_iter_count(&iter));
	if (!ret) {
		/*
		 * Open-code file_start_write here to grab freeze protection,
		 * which will be released by another thread in
		 * io_complete_rw().  Fool lockdep by telling it the lock got
		 * released so that it doesn't complain about the held lock when
		 * we return to userspace.
		 */
		if (S_ISREG(file_inode(file)->i_mode)) {
			__sb_start_write(file_inode(file)->i_sb,
						SB_FREEZE_WRITE, true);
			__sb_writers_release(file_inode(file)->i_sb,
						SB_FREEZE_WRITE);
		}
		kiocb->ki_flags |= IOCB_WRITE;
		io_rw_done(kiocb, call_write_iter(file, kiocb, &iter));
	}
	kfree(iovec);
out_fput:
	if (unlikely(ret))
		fput(file);
	return ret;
}

/*
 * IORING_OP_NOP just posts a completion event, nothing else.
 */
static int io_nop(struct io_kiocb *req, u64 user_data)
{
	struct io_ring_ctx *ctx = req->ctx;
	long err = 0;

	/*
	 * Twilight zone - it's possible that someone issued an opcode that
	 * has a file attached, then got -EAGAIN on submission, and changed
	 * the sqe before we retried it from async context. Avoid dropping
	 * a file reference for this malicious case, and flag the error.
	 */
	if (req->rw.ki_filp) {
		err = -EBADF;
		fput(req->rw.ki_filp);
	}
	io_cqring_add_event(ctx, user_data, err, 0);
	io_free_req(req);
	return 0;
}

static int __io_submit_sqe(struct io_ring_ctx *ctx, struct io_kiocb *req,
			   const struct sqe_submit *s, bool force_nonblock)
{
	ssize_t ret;
	int opcode;
	req->user_data = READ_ONCE(s->sqe->user_data);

	opcode = READ_ONCE(s->sqe->opcode);
	switch (opcode) {
	case IORING_OP_NOP:
		ret = io_nop(req, req->user_data);
		break;
	case IORING_OP_READV:
		ret = io_read(req, s, force_nonblock);
		break;
	case IORING_OP_WRITEV:
		ret = io_write(req, s, force_nonblock);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void io_sq_wq_submit_work(struct work_struct *work)
{
	struct io_kiocb *req = container_of(work, struct io_kiocb, work);
	struct sqe_submit *s = &req->submit;
	const struct io_uring_sqe *sqe = s->sqe;
	struct io_ring_ctx *ctx = req->ctx;
	mm_segment_t old_fs = get_fs();
	int ret;

	 /* Ensure we clear previously set forced non-block flag */
	req->flags &= ~REQ_F_FORCE_NONBLOCK;
	req->rw.ki_flags &= ~IOCB_NOWAIT;

	if (!mmget_not_zero(ctx->sqo_mm)) {
		ret = -EFAULT;
		goto err;
	}

	use_mm(ctx->sqo_mm);
	set_fs(USER_DS);
	s->has_user = true;

	ret = __io_submit_sqe(ctx, req, s, false);

	set_fs(old_fs);
	unuse_mm(ctx->sqo_mm);
	mmput(ctx->sqo_mm);
err:
	if (ret) {
		io_cqring_add_event(ctx, sqe->user_data, ret, 0);
		io_free_req(req);
	}

	/* async context always use a copy of the sqe */
	kfree(sqe);
}

static int io_submit_sqe(struct io_ring_ctx *ctx, struct sqe_submit *s)
{
	struct io_kiocb *req;
	ssize_t ret;

	req = io_get_req(ctx);
	req->rw.ki_filp = NULL;
	ret = __io_submit_sqe(ctx, req, s, true);
	if (ret == -EAGAIN) {
		struct io_uring_sqe *sqe_copy;
		// 如果是非阻塞的话，将该io任务交给workqueue完成
		sqe_copy = kmalloc(sizeof(*sqe_copy), GFP_KERNEL);
		if (sqe_copy) {
			memcpy(sqe_copy, s->sqe, sizeof(*sqe_copy));
			s->sqe = sqe_copy;

			memcpy(&req->submit, s, sizeof(*s));
			INIT_WORK(&req->work, io_sq_wq_submit_work);
			queue_work(ctx->sqo_wq, &req->work);
			ret = 0;
		}
	}
	return ret;
}

static void io_commit_sqring(struct io_ring_ctx *ctx)
{
	struct io_sq_ring *ring = ctx->sq_ring;

	if (ctx->cached_sq_head != READ_ONCE(ring->r.head)) {
		/*
		 * Ensure any loads from the SQEs are done at this point,
		 * since once we write the new head, the application could
		 * write new data to them.
		 */
		smp_store_release(&ring->r.head, ctx->cached_sq_head);

		/*
		 * write side barrier of head update, app has read side. See
		 * comment at the top of this file
		 */
		smp_wmb();
	}
}

/*
 * Undo last io_get_sqring()
 */
static void io_drop_sqring(struct io_ring_ctx *ctx)
{
	ctx->cached_sq_head--;
}

/*
 * Fetch an sqe, if one is available. Note that s->sqe will point to memory
 * that is mapped by userspace. This means that care needs to be taken to
 * ensure that reads are stable, as we cannot rely on userspace always
 * being a good citizen. If members of the sqe are validated and then later
 * used, it's important that those reads are done through READ_ONCE() to
 * prevent a re-load down the line.
 */
static bool io_get_sqring(struct io_ring_ctx *ctx, struct sqe_submit *s)
{
	struct io_sq_ring *ring = ctx->sq_ring;
	unsigned head;

	/*
	 * The cached sq head (or cq tail) serves two purposes:
	 *
	 * 1) allows us to batch the cost of updating the user visible
	 *    head updates.
	 * 2) allows the kernel side to track the head on its own, even
	 *    though the application is the one updating it.
	 */
	head = ctx->cached_sq_head;
	/* See comment at the top of this file */
	smp_rmb();
	// 
	if (head == READ_ONCE(ring->r.tail))
		return false;

	head = READ_ONCE(ring->array[head & ctx->sq_mask]);
	if (head < ctx->sq_entries) {
		s->index = head;
		s->sqe = &ctx->sq_sqes[head];
		ctx->cached_sq_head++;
		return true;
	}

	/* drop invalid entries */
	ctx->cached_sq_head++;
	ring->dropped++;
	/* See comment at the top of this file */
	smp_wmb();
	return false;
}

static int io_ring_submit(struct io_ring_ctx *ctx, unsigned int to_submit)
{
	int i, ret = 0, submit = 0;
	struct blk_plug plug;
	for (i = 0; i < to_submit; i++) {
		struct sqe_submit s;
		// 从ctx->sqes取出一个用户已经放进去的io任务
		if (!io_get_sqring(ctx, &s))
			break;
		s.has_user = true;
		ret = io_submit_sqe(ctx, &s);
		if (ret) {
			io_drop_sqring(ctx);
			break;
		}
		submit++;
	}
	io_commit_sqring(ctx);
	if (to_submit > IO_PLUG_THRESHOLD)
		blk_finish_plug(&plug);
	return submit ? submit : ret;
}

static unsigned io_cqring_events(struct io_cq_ring *ring)
{
	return READ_ONCE(ring->r.tail) - READ_ONCE(ring->r.head);
}

/*
 * Wait until events become available, if we don't already have some. The
 * application must reap them itself, as they reside on the shared cq ring.
 */
static int io_cqring_wait(struct io_ring_ctx *ctx, int min_events,
			  const sigset_t __user *sig, size_t sigsz)
{
	struct io_cq_ring *ring = ctx->cq_ring;
	sigset_t ksigmask, sigsaved;
	DEFINE_WAIT(wait);
	int ret;

	/* See comment at the top of this file */
	smp_rmb();
	if (io_cqring_events(ring) >= min_events)
		return 0;

	if (sig) {
		ret = set_user_sigmask(sig, &ksigmask, &sigsaved, sigsz);
		if (ret)
			return ret;
	}

	do {
		prepare_to_wait(&ctx->wait, &wait, TASK_INTERRUPTIBLE);

		ret = 0;
		/* See comment at the top of this file */
		smp_rmb();
		if (io_cqring_events(ring) >= min_events)
			break;

		schedule();

		ret = -EINTR;
		if (signal_pending(current))
			break;
	} while (1);

	finish_wait(&ctx->wait, &wait);

	if (sig)
		restore_user_sigmask(sig, &sigsaved);

	return READ_ONCE(ring->r.head) == READ_ONCE(ring->r.tail) ? ret : 0;
}

static int io_sq_offload_start(struct io_ring_ctx *ctx)
{
	int ret;
	mmgrab(current->mm);
	ctx->sqo_mm = current->mm;
	/* Do QD, or 2 * CPUS, whatever is smallest */
	ctx->sqo_wq = alloc_workqueue("io_ring-wq", WQ_UNBOUND | WQ_FREEZABLE,
			min(ctx->sq_entries - 1, 2 * num_online_cpus()));
	return 0;
}

static int io_account_mem(struct user_struct *user, unsigned long nr_pages)
{
	unsigned long page_limit, cur_pages, new_pages;

	/* Don't allow more pages than we can safely lock */
	page_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;

	do {
		cur_pages = atomic_long_read(&user->locked_vm);
		new_pages = cur_pages + nr_pages;
		if (new_pages > page_limit)
			return -ENOMEM;
	} while (atomic_long_cmpxchg(&user->locked_vm, cur_pages,
					new_pages) != cur_pages);

	return 0;
}

static void *io_mem_alloc(size_t size)
{
	gfp_t gfp_flags = GFP_KERNEL | __GFP_ZERO | __GFP_NOWARN | __GFP_COMP |
				__GFP_NORETRY;

	return (void *) __get_free_pages(gfp_flags, get_order(size));
}

static unsigned long ring_pages(unsigned sq_entries, unsigned cq_entries)
{
	struct io_sq_ring *sq_ring;
	struct io_cq_ring *cq_ring;
	size_t bytes;

	bytes = struct_size(sq_ring, array, sq_entries);
	bytes += array_size(sizeof(struct io_uring_sqe), sq_entries);
	bytes += struct_size(cq_ring, cqes, cq_entries);

	return (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
}


static __poll_t io_uring_poll(struct file *file, poll_table *wait)
{
	struct io_ring_ctx *ctx = file->private_data;
	__poll_t mask = 0;

	poll_wait(file, &ctx->cq_wait, wait);
	/* See comment at the top of this file */
	smp_rmb();
	if (READ_ONCE(ctx->sq_ring->r.tail) + 1 != ctx->cached_sq_head)
		mask |= EPOLLOUT | EPOLLWRNORM;
	if (READ_ONCE(ctx->cq_ring->r.head) != ctx->cached_cq_tail)
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

static int io_uring_fasync(int fd, struct file *file, int on)
{
	struct io_ring_ctx *ctx = file->private_data;

	return fasync_helper(fd, file, on, &ctx->cq_fasync);
}

static void io_ring_ctx_wait_and_kill(struct io_ring_ctx *ctx)
{
	mutex_lock(&ctx->uring_lock);
	percpu_ref_kill(&ctx->refs);
	mutex_unlock(&ctx->uring_lock);

	wait_for_completion(&ctx->ctx_done);
	io_ring_ctx_free(ctx);
}

static int io_uring_mmap(struct file *file, struct vm_area_struct *vma)
{
	loff_t offset = (loff_t) vma->vm_pgoff << PAGE_SHIFT;
	unsigned long sz = vma->vm_end - vma->vm_start;
	struct io_ring_ctx *ctx = file->private_data;
	unsigned long pfn;
	struct page *page;
	void *ptr;
	// 三种不同的内存空间映射
	switch (offset) {
	case IORING_OFF_SQ_RING:
		ptr = ctx->sq_ring;
		break;
	case IORING_OFF_SQES:
		ptr = ctx->sq_sqes;
		break;
	case IORING_OFF_CQ_RING:
		ptr = ctx->cq_ring;
		break;
	default:
		return -EINVAL;
	}

	page = virt_to_head_page(ptr);
	if (sz > (PAGE_SIZE << compound_order(page)))
		return -EINVAL;

	pfn = virt_to_phys(ptr) >> PAGE_SHIFT;
	// 将该内存映射到用户空间
	return remap_pfn_range(vma, vma->vm_start, pfn, sz, vma->vm_page_prot);
}

// to_submit:本次提交的sqe个数
SYSCALL_DEFINE6(io_uring_enter, unsigned int, fd, u32, to_submit,
		u32, min_complete, u32, flags, const sigset_t __user *, sig,
		size_t, sigsz)
{
	struct io_ring_ctx *ctx;
	long ret = -EBADF;
	int submitted = 0;
	struct fd f;
	f = fdget(fd);
	ctx = f.file->private_data;
	if (to_submit) {
		to_submit = min(to_submit, ctx->sq_entries);

		mutex_lock(&ctx->uring_lock);
		// 
		submitted = io_ring_submit(ctx, to_submit);
		mutex_unlock(&ctx->uring_lock);

		if (submitted < 0)
			goto out_ctx;
	}
	// 是否等待完成指定数量的任务
	if (flags & IORING_ENTER_GETEVENTS) {
		min_complete = min(min_complete, ctx->cq_entries);

		/*
		 * The application could have included the 'to_submit' count
		 * in how many events it wanted to wait for. If we failed to
		 * submit the desired count, we may need to adjust the number
		 * of events to poll/wait for.
		 */
		if (submitted < to_submit)
			min_complete = min_t(unsigned, submitted, min_complete);
		// waitqueue机制
		ret = io_cqring_wait(ctx, min_complete, sig, sigsz);
	}

out_ctx:
	io_ring_drop_ctx_refs(ctx, 1);
out_fput:
	fdput(f);
	return submitted ? submitted : ret;
}

static const struct file_operations io_uring_fops = {
	.release	= io_uring_release,
	.mmap		= io_uring_mmap,
	.poll		= io_uring_poll,
	.fasync		= io_uring_fasync,
};

static int io_allocate_scq_urings(struct io_ring_ctx *ctx,
				  struct io_uring_params *p)
{
	struct io_sq_ring *sq_ring;
	struct io_cq_ring *cq_ring;
	size_t size;
	// 为io提交队列分配内存空间
	sq_ring = io_mem_alloc(struct_size(sq_ring, array, p->sq_entries));
	ctx->sq_ring = sq_ring;
	// 掩码
	sq_ring->ring_mask = p->sq_entries - 1;
	// io提交队列容量
	sq_ring->ring_entries = p->sq_entries;
	// 同样信息保存到io_ring_ctx上下文中
	ctx->sq_mask = sq_ring->ring_mask;
	ctx->sq_entries = sq_ring->ring_entries;
	// 为sqe分配内存空间，前面的io_sq_ring中的array只是保存sqe_array中的索引，
	// 主要是因为io完成和提交的次序并不能够确定
	size = array_size(sizeof(struct io_uring_sqe), p->sq_entries);
	ctx->sq_sqes = io_mem_alloc(size);
	// 为io完成队列分配内存空间
	cq_ring = io_mem_alloc(struct_size(cq_ring, cqes, p->cq_entries));
	ctx->cq_ring = cq_ring;
	cq_ring->ring_mask = p->cq_entries - 1;
	cq_ring->ring_entries = p->cq_entries;
	ctx->cq_mask = cq_ring->ring_mask;
	ctx->cq_entries = cq_ring->ring_entries;
	return 0;
}

static int io_uring_get_fd(struct io_ring_ctx *ctx)
{
	struct file *file;
	int ret;

	ret = get_unused_fd_flags(O_RDWR | O_CLOEXEC);
	// 创建文件
	file = anon_inode_getfile("[io_uring]", &io_uring_fops, ctx,
					O_RDWR | O_CLOEXEC);
	// 文件和fd绑定
	fd_install(ret, file);
	return ret;
}

static int io_uring_create(unsigned entries, struct io_uring_params *p)
{
	struct user_struct *user = NULL;
	struct io_ring_ctx *ctx;
	bool account_mem;
	int ret;

	/*
	 * Use twice as many entries for the CQ ring. It's possible for the
	 * application to drive a higher depth than the size of the SQ ring,
	 * since the sqes are only used at submission time. This allows for
	 * some flexibility in overcommitting a bit.
	 */
    // todo
	p->sq_entries = roundup_pow_of_two(entries);
	p->cq_entries = 2 * p->sq_entries;

	user = get_uid(current_user());
	account_mem = !capable(CAP_IPC_LOCK);
	// todo
	if (account_mem) {
		ret = io_account_mem(user,
				ring_pages(p->sq_entries, p->cq_entries));
		if (ret) {
			free_uid(user);
			return ret;
		}
	}
	ctx = io_ring_ctx_alloc(p);
	ctx->compat = in_compat_syscall();
	ctx->account_mem = account_mem;
	ctx->user = user;
	// 分配三个内存空间：io提交队列、sqe array、io完成队列
	ret = io_allocate_scq_urings(ctx, p);
	// 创建消耗任务的内核线程
	ret = io_sq_offload_start(ctx);
	// 将ctx同文件绑定，返回该文件的fd
	ret = io_uring_get_fd(ctx);
	memset(&p->sq_off, 0, sizeof(p->sq_off));
	// 应用程序通过offset获取成员
	p->sq_off.head = offsetof(struct io_sq_ring, r.head);
	p->sq_off.tail = offsetof(struct io_sq_ring, r.tail);
	p->sq_off.ring_mask = offsetof(struct io_sq_ring, ring_mask);
	p->sq_off.ring_entries = offsetof(struct io_sq_ring, ring_entries);
	p->sq_off.flags = offsetof(struct io_sq_ring, flags);
	p->sq_off.dropped = offsetof(struct io_sq_ring, dropped);
	p->sq_off.array = offsetof(struct io_sq_ring, array);

	memset(&p->cq_off, 0, sizeof(p->cq_off));
	p->cq_off.head = offsetof(struct io_cq_ring, r.head);
	p->cq_off.tail = offsetof(struct io_cq_ring, r.tail);
	p->cq_off.ring_mask = offsetof(struct io_cq_ring, ring_mask);
	p->cq_off.ring_entries = offsetof(struct io_cq_ring, ring_entries);
	p->cq_off.overflow = offsetof(struct io_cq_ring, overflow);
	p->cq_off.cqes = offsetof(struct io_cq_ring, cqes);
	return ret;
}

// entries:表示sqe的元素个数
// ret:fd
static long io_uring_setup(u32 entries, struct io_uring_params __user *params)
{
	struct io_uring_params p;
	long ret;
	if (copy_from_user(&p, params, sizeof(p)))
		return -EFAULT;
	ret = io_uring_create(entries, &p);
	if (copy_to_user(params, &p, sizeof(p)))
		return -EFAULT;
	return ret;
}

SYSCALL_DEFINE2(io_uring_setup, u32, entries,
		struct io_uring_params __user *, params)
{
	return io_uring_setup(entries, params);
}

static int __init io_uring_init(void)
{
	req_cachep = KMEM_CACHE(io_kiocb, SLAB_HWCACHE_ALIGN | SLAB_PANIC);
	return 0;
};
__initcall(io_uring_init);

