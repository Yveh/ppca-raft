// Generated by the gRPC C++ plugin.
// If you make any local change, they will be lost.
// source: raft.proto

#include "raft.pb.h"
#include "raft.grpc.pb.h"

#include <functional>
#include <grpcpp/impl/codegen/async_stream.h>
#include <grpcpp/impl/codegen/async_unary_call.h>
#include <grpcpp/impl/codegen/channel_interface.h>
#include <grpcpp/impl/codegen/client_unary_call.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/impl/codegen/method_handler_impl.h>
#include <grpcpp/impl/codegen/rpc_service_method.h>
#include <grpcpp/impl/codegen/server_callback.h>
#include <grpcpp/impl/codegen/service_type.h>
#include <grpcpp/impl/codegen/sync_stream.h>

static const char* RAFT_method_names[] = {
  "/RAFT/SayHello",
  "/RAFT/RequestVote",
};

std::unique_ptr< RAFT::Stub> RAFT::NewStub(const std::shared_ptr< ::grpc::ChannelInterface>& channel, const ::grpc::StubOptions& options) {
  (void)options;
  std::unique_ptr< RAFT::Stub> stub(new RAFT::Stub(channel));
  return stub;
}

RAFT::Stub::Stub(const std::shared_ptr< ::grpc::ChannelInterface>& channel)
  : channel_(channel), rpcmethod_SayHello_(RAFT_method_names[0], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  , rpcmethod_RequestVote_(RAFT_method_names[1], ::grpc::internal::RpcMethod::NORMAL_RPC, channel)
  {}

::grpc::Status RAFT::Stub::SayHello(::grpc::ClientContext* context, const ::HelloRequest& request, ::HelloReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_SayHello_, context, request, response);
}

void RAFT::Stub::experimental_async::SayHello(::grpc::ClientContext* context, const ::HelloRequest* request, ::HelloReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_SayHello_, context, request, response, std::move(f));
}

void RAFT::Stub::experimental_async::SayHello(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::HelloReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_SayHello_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::HelloReply>* RAFT::Stub::AsyncSayHelloRaw(::grpc::ClientContext* context, const ::HelloRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::HelloReply>::Create(channel_.get(), cq, rpcmethod_SayHello_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::HelloReply>* RAFT::Stub::PrepareAsyncSayHelloRaw(::grpc::ClientContext* context, const ::HelloRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::HelloReply>::Create(channel_.get(), cq, rpcmethod_SayHello_, context, request, false);
}

::grpc::Status RAFT::Stub::RequestVote(::grpc::ClientContext* context, const ::RequestVoteRequest& request, ::RequestVoteReply* response) {
  return ::grpc::internal::BlockingUnaryCall(channel_.get(), rpcmethod_RequestVote_, context, request, response);
}

void RAFT::Stub::experimental_async::RequestVote(::grpc::ClientContext* context, const ::RequestVoteRequest* request, ::RequestVoteReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_RequestVote_, context, request, response, std::move(f));
}

void RAFT::Stub::experimental_async::RequestVote(::grpc::ClientContext* context, const ::grpc::ByteBuffer* request, ::RequestVoteReply* response, std::function<void(::grpc::Status)> f) {
  return ::grpc::internal::CallbackUnaryCall(stub_->channel_.get(), stub_->rpcmethod_RequestVote_, context, request, response, std::move(f));
}

::grpc::ClientAsyncResponseReader< ::RequestVoteReply>* RAFT::Stub::AsyncRequestVoteRaw(::grpc::ClientContext* context, const ::RequestVoteRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::RequestVoteReply>::Create(channel_.get(), cq, rpcmethod_RequestVote_, context, request, true);
}

::grpc::ClientAsyncResponseReader< ::RequestVoteReply>* RAFT::Stub::PrepareAsyncRequestVoteRaw(::grpc::ClientContext* context, const ::RequestVoteRequest& request, ::grpc::CompletionQueue* cq) {
  return ::grpc::internal::ClientAsyncResponseReaderFactory< ::RequestVoteReply>::Create(channel_.get(), cq, rpcmethod_RequestVote_, context, request, false);
}

RAFT::Service::Service() {
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RAFT_method_names[0],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RAFT::Service, ::HelloRequest, ::HelloReply>(
          std::mem_fn(&RAFT::Service::SayHello), this)));
  AddMethod(new ::grpc::internal::RpcServiceMethod(
      RAFT_method_names[1],
      ::grpc::internal::RpcMethod::NORMAL_RPC,
      new ::grpc::internal::RpcMethodHandler< RAFT::Service, ::RequestVoteRequest, ::RequestVoteReply>(
          std::mem_fn(&RAFT::Service::RequestVote), this)));
}

RAFT::Service::~Service() {
}

::grpc::Status RAFT::Service::SayHello(::grpc::ServerContext* context, const ::HelloRequest* request, ::HelloReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}

::grpc::Status RAFT::Service::RequestVote(::grpc::ServerContext* context, const ::RequestVoteRequest* request, ::RequestVoteReply* response) {
  (void) context;
  (void) request;
  (void) response;
  return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED, "");
}


