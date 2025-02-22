#pragma once
// TODO: This needs some namespace
#include <node/v8.h>

#include "zeek/Val.h"

const int ZEEKJS_ATTR_LOG = 1;

class ZeekValWrap;

// Helper class for wrapping a zeek::ValPtr into a v8::Object.
class ZeekValWrapper {
 public:
  ZeekValWrapper(v8::Isolate* isolate);

  // Get the record field offset for this field, or -1 if not existing.
  int GetRecordFieldOffset(const zeek::RecordTypePtr& rt,
                           const v8::Local<v8::Name>& property);

  // Return a BigInt object given v.
  v8::Local<v8::BigInt> GetBigInt(zeek_uint_t v);

  // Wrap any zeek::ValPtr as object rather than converting to primitive types.
  v8::Local<v8::Object> WrapAsObject(const zeek::ValPtr& vp, int attr_mask = 0);

  // Wrap anything into a v8::Value. Some types are converted
  // directly like strings and numbers. Others return a "proxy"
  // object which keeps a reference to the original ValPtr and
  // uses below callbacks.
  //
  v8::Local<v8::Value> Wrap(const zeek::ValPtr& vp, int attr_mask = 0);

  struct Result {
    bool ok;
    zeek::ValPtr val;
    std::string error;
  };

  // Convert a v8::Value to a ValPtr of the given type. If the
  // conversion fails, e.g. if type is IPAddr, but v8_val not
  // a string that conforms to an IP, returns a Result with an
  // error message.
  //
  Result ToZeekVal(v8::Local<v8::Value> v8_val, const zeek::TypePtr& type);

  // Callbacks used for Zeek tables.
  static void ZeekTableGetter(v8::Local<v8::Name> property,
                              const v8::PropertyCallbackInfo<v8::Value>& info);
  static void ZeekTableSetter(v8::Local<v8::Name> property,
                              v8::Local<v8::Value> v8_val,
                              const v8::PropertyCallbackInfo<v8::Value>& info);
  static void ZeekTableIndexGetter(uint32_t index,
                                   const v8::PropertyCallbackInfo<v8::Value>& info);
  static void ZeekTableIndexSetter(uint32_t index,
                                   v8::Local<v8::Value> v8_val,
                                   const v8::PropertyCallbackInfo<v8::Value>& info);
  static void ZeekTableEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info);

  // Callbacks used for Zeek records.
  static void ZeekRecordGetter(v8::Local<v8::Name> property,
                               const v8::PropertyCallbackInfo<v8::Value>& info);
  static void ZeekRecordSetter(v8::Local<v8::Name> property,
                               v8::Local<v8::Value> v8_val,
                               const v8::PropertyCallbackInfo<v8::Value>& info);
  static void ZeekRecordQuery(v8::Local<v8::Name> property,
                              const v8::PropertyCallbackInfo<v8::Integer>& info);
  static void ZeekRecordEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info);

  // String conversion helpers
  v8::Local<v8::String> v8_str_intern(const char* s);
  v8::Local<v8::String> v8_str(const char* s);

  // Returns the v8::Local<v8::Private> to mark objects wrapping Zeek Vals.
  v8::Local<v8::Private> GetWrapPrivateKey(v8::Isolate* isolate) const {
    return wrap_private_key_.Get(isolate);
  }

  // Attempt to unwrap the given object if it is a ZeekValWrap.
  //
  // Returns true if unwrapping was successful in which case wrap is set.
  bool Unwrap(v8::Isolate* isolate, v8::Local<v8::Object> obj, ZeekValWrap** wrap);

 private:
  v8::Isolate* isolate_;
  v8::Global<v8::ObjectTemplate> record_template_;
  v8::Global<v8::ObjectTemplate> table_template_;
  v8::Global<v8::String> port_str_;
  v8::Global<v8::String> proto_str_;
  v8::Global<v8::String> toJSON_str_;
  // v8::Objects that have a private property with this key
  // are ZeekValWraps and we can Unwrap them directly. This
  // is done much nicer in node with napi_type_tag_object().
  v8::Global<v8::Private> wrap_private_key_;

  // RecordType::FieldOffset is slow and the result is static. Cache it.
  //
  // XXX: This should be possible with operator< overloading?
  struct RecordTypeLess {
    bool operator()(const zeek::RecordTypePtr& l, const zeek::RecordTypePtr& r) const {
      return l.get() < r.get();
    }
  };

  // We identity hashes to offsets as well as strings to offsets in case
  // there are collisions. We could probably ignore the latter.
  using IdentityHashOffsetMap = std::map<int, int>;
  using NameOffsetMap = std::map<std::string, int>;
  using RecordIdentityHashOffsetMap =
      std::map<zeek::RecordTypePtr, IdentityHashOffsetMap, RecordTypeLess>;
  using RecordNameOffsetMap =
      std::map<zeek::RecordTypePtr, NameOffsetMap, RecordTypeLess>;

  RecordIdentityHashOffsetMap record_field_identity_hash_offsets;
  RecordNameOffsetMap record_field_name_offsets;

  void InitRecordOffsets(const zeek::RecordTypePtr& rt);

  // Zeek is keeping just 4096 counts, but SSL extensions or cipher codes
  // are all 16 bit, so pre-allocate 2**16 instead. The static memory usage
  // of the array is 512KB. Peanuts ;-)
  //
  // The entry are constructed lazily though.
  std::array<v8::Persistent<v8::BigInt>, 65536> persistent_bigints_;
};

// Wraps a zeek::ValPtr with enough info to continue wrapping.
//
// This is used as internal field and hooked up with garbage collection
// via SetWeak().
//
class ZeekValWrap {
 public:
  v8::Local<v8::Object> GetHandle(v8::Isolate* isolate) {
    return persistent_obj_.Get(isolate);
  }

  ZeekValWrapper* GetWrapper() { return wrapper_; }
  zeek::Val* GetVal() { return vp_; }
  const int GetAttrMask() { return attr_mask_; };

  static ZeekValWrap* Make(v8::Isolate* isolate,
                           ZeekValWrapper* wrapper,
                           v8::Local<v8::Object> record_obj,
                           zeek::Val* vp,
                           int attr_mask = 0);

 private:
  ZeekValWrap(v8::Isolate* isolate,
              ZeekValWrapper* wrapper,
              v8::Local<v8::Object> record_obj,
              zeek::Val* vp,
              int attr_mask);

  v8::Persistent<v8::Object> persistent_obj_;
  ZeekValWrapper* wrapper_ = nullptr;
  zeek::Val* vp_ = nullptr;
  int attr_mask_ = 0;

  // Callback from V8 to let us know that the Object isn't referenced
  // anymore and we can now Unref() the Val we held on to.
  static void ZeekValWrap_WeakCallback(const v8::WeakCallbackInfo<ZeekValWrap>& data);
};
