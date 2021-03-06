

#include "serverconnection.h"
#include <errno.h>
#include "tools/serializer.h"

namespace reindexer {
namespace net {
namespace cproto {

const auto kCProtoTimeoutSec = 300.;

ServerConnection::ServerConnection(int fd, ev::dynamic_loop &loop, Dispatcher &dispatcher)
	: net::ConnectionST(fd, loop), dispatcher_(dispatcher) {
	timeout_.start(kCProtoTimeoutSec);
	callback(io_, ev::READ);
}

bool ServerConnection::Restart(int fd) {
	restart(fd);
	respSent_ = false;
	callback(io_, ev::READ);
	timeout_.start(kCProtoTimeoutSec);
	return true;
}

void ServerConnection::Attach(ev::dynamic_loop &loop) {
	if (!attached_) {
		attach(loop);
		timeout_.start(kCProtoTimeoutSec);
	}
}

void ServerConnection::Detach() {
	if (attached_) detach();
}

void ServerConnection::onClose() {
	if (dispatcher_.onClose_) {
		Stat stat;
		Context ctx;
		ctx.call = nullptr;
		ctx.writer = this;
		ctx.stat = stat;

		dispatcher_.onClose_(ctx, errOK);
	}
	clientData_.reset();
}

void ServerConnection::handleRPC(Context &ctx) {
	Error err = dispatcher_.handle(ctx);

	if (!respSent_) {
		responceRPC(ctx, err, Args());
	}
}

void ServerConnection::onRead() {
	CProtoHeader hdr;

	while (!closeConn_) {
		Context ctx;
		ctx.call = nullptr;
		ctx.writer = this;

		auto len = rdBuf_.peek(reinterpret_cast<char *>(&hdr), sizeof(hdr));

		if (len < sizeof(hdr)) return;
		if (hdr.magic != kCprotoMagic) {
			responceRPC(ctx, Error(errParams, "Invalid cproto magic %08x", int(hdr.magic)), Args());
			closeConn_ = true;
			return;
		}

		if (hdr.version < kCprotoVersion) {
			responceRPC(ctx,
						Error(errParams, "Unsupported cproto version %04x. This server expects reindexer client v1.9.8+", int(hdr.version)),
						Args());
			closeConn_ = true;
			return;
		}

		if (hdr.len + sizeof(hdr) > rdBuf_.capacity()) {
			rdBuf_.reserve(hdr.len + sizeof(hdr) + 0x1000);
		}

		if (hdr.len + sizeof(hdr) > rdBuf_.size()) {
			if (!rdBuf_.size()) rdBuf_.clear();
			return;
		}

		rdBuf_.erase(sizeof(hdr));

		auto it = rdBuf_.tail();
		if (it.size() < hdr.len) {
			rdBuf_.unroll();
			it = rdBuf_.tail();
		}
		assert(it.size() >= hdr.len);

		ctx.call = &call_;
		try {
			ctx.call->cmd = CmdCode(hdr.cmd);
			ctx.call->seq = hdr.seq;
			Serializer ser(it.data(), hdr.len);
			ctx.call->args.Unpack(ser);
			handleRPC(ctx);
		} catch (const Error &err) {
			// Execption occurs on unrecoverble error. Send responce, and drop connection
			fprintf(stderr, "drop connect, reason: %s\n", err.what().c_str());
			responceRPC(ctx, err, Args());
			closeConn_ = true;
		}

		respSent_ = false;
		rdBuf_.erase(hdr.len);
		timeout_.start(kCProtoTimeoutSec);
	}
}

void ServerConnection::responceRPC(Context &ctx, const Error &status, const Args &args) {
	if (respSent_) {
		fprintf(stderr, "Warning - RPC responce already sent\n");
		return;
	}

	WrSerializer ser(wrBuf_.get_chunk());

	CProtoHeader hdr;
	hdr.len = 0;
	hdr.magic = kCprotoMagic;
	hdr.version = kCprotoVersion;
	if (ctx.call != nullptr) {
		hdr.cmd = ctx.call->cmd;
		hdr.seq = ctx.call->seq;
	} else {
		hdr.cmd = 0;
		hdr.seq = 0;
	}

	ser.Write(string_view(reinterpret_cast<char *>(&hdr), sizeof(hdr)));
	ser.PutVarUint(status.code());
	ser.PutVString(status.what());
	args.Pack(ser);
	reinterpret_cast<CProtoHeader *>(ser.Buf())->len = ser.Len() - sizeof(hdr);
	wrBuf_.write(ser.DetachChunk());

	respSent_ = true;
	// if (canWrite_) {
	// 	write_cb();
	// }

	if (dispatcher_.logger_ != nullptr) {
		dispatcher_.logger_(ctx, status, args);
	}
}

}  // namespace cproto
}  // namespace net
}  // namespace reindexer
