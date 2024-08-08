/*
   Copyright (c) 2024, ApeCloud Inc Holding Limited.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys/objstore/aliyun_oss.h"
#include <fstream>
#include <iostream>
#include <ctime>
#include <iomanip>

namespace objstore {

namespace {

Errors aliyun_oss_error_to_se_error(const AlibabaCloud::OSS::OssError &error) {
  static std::unordered_map<std::string, AliyunOSSErrorCode> errorMap = {
    {"AccessDenied", AliyunOSSErrorCode::AccessDenied},
    {"NoSuchBucket", AliyunOSSErrorCode::NoSuchBucket},
    {"NoSuchKey", AliyunOSSErrorCode::NoSuchKey},
    {"BucketAlreadyExists", AliyunOSSErrorCode::BucketAlreadyExists},
    {"SymlinkTargetNotExist", AliyunOSSErrorCode::SymlinkTargetNotExist},
    {"InvalidObjectState", AliyunOSSErrorCode::InvalidObjectState},
    {"InvalidArgument", AliyunOSSErrorCode::InvalidArgument},
    {"TooManyBuckets", AliyunOSSErrorCode::TooManyBuckets}
  };
  if (errorMap.find(error.Code()) != errorMap.end()) {
    switch (errorMap.at(error.Code())) {
      case AliyunOSSErrorCode::AccessDenied:
        return Errors::SE_ACCESS_DENIED;
      case AliyunOSSErrorCode::NoSuchBucket:
        return Errors::SE_NO_SUCH_BUCKET;
      case AliyunOSSErrorCode::NoSuchKey:
        return Errors::SE_NO_SUCH_KEY;
      case AliyunOSSErrorCode::BucketAlreadyExists:
        return Errors::SE_BUCKET_ALREADY_EXISTS;
      case AliyunOSSErrorCode::SymlinkTargetNotExist:
        return Errors::SE_SYMLINK_TARGET_NOT_EXIST;
      case AliyunOSSErrorCode::InvalidObjectState:
        return Errors::SE_INVALID_OBJECT_STATE;
      case AliyunOSSErrorCode::InvalidArgument:
        return Errors::SE_OBJSTORE_INVALID_ARGUMENT;
      case AliyunOSSErrorCode::TooManyBuckets:
        return Errors::SE_TOO_MANY_BUCKETS;
      default:
        return Errors::CLOUD_PROVIDER_UNRECOVERABLE_ERROR;
    }
  }
  return Errors::CLOUD_PROVIDER_UNRECOVERABLE_ERROR;
}

int64_t convertTimeStr2Int64(const std::string& timestamp) {
    std::tm tm = {};
    std::stringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    auto time_point = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()).count();
}

}  // namespace

Status AliyunOssObjectStore::create_bucket(const std::string_view &bucket) {
  AlibabaCloud::OSS::CreateBucketRequest request((std::string(bucket)));
  auto outcome = oss_client_.CreateBucket(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  return Status();
}

Status AliyunOssObjectStore::delete_bucket(const std::string_view &bucket) {
  AlibabaCloud::OSS::DeleteBucketRequest request((std::string(bucket)));
  auto outcome = oss_client_.DeleteBucket(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  return Status();
}

Status AliyunOssObjectStore::put_object_from_file(
    const std::string_view &bucket, const std::string_view &key,
    const std::string_view &data_file_path) {
  std::shared_ptr<std::iostream> content = std::make_shared<std::fstream>(
      data_file_path.data(), std::ios::in | std::ios::binary);
  if (nullptr == content) {
    return Status(Errors::SE_IO_ERROR, 0, "failed to open file for put object");
  }
  AlibabaCloud::OSS::PutObjectRequest request(std::string(bucket),
                                              std::string(key), content);
  auto outcome = oss_client_.PutObject(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  return Status();
}

Status AliyunOssObjectStore::get_object_to_file(
    const std::string_view &bucket, const std::string_view &key,
    const std::string_view &output_file_path) {
  AlibabaCloud::OSS::GetObjectRequest request((std::string(bucket)),
                                              std::string(key));

  AlibabaCloud::OSS::IOStreamFactory factory = [=]() {
    return std::make_shared<std::fstream>(
        output_file_path.data(), std::ios_base::out | std::ios_base::in |
                                     std::ios_base::trunc |
                                     std::ios_base::binary);
  };
  request.setResponseStreamFactory(factory);
  auto outcome = oss_client_.GetObject(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  return Status();
}

Status AliyunOssObjectStore::put_object(const std::string_view &bucket,
                                        const std::string_view &key,
                                        const std::string_view &data) {
  std::shared_ptr<std::iostream> content = std::make_shared<std::stringstream>(std::string(data));
  if (content == nullptr) {
    return Status(SE_IO_ERROR, 0, "failed to allocate memory for put object.");
  }
  AlibabaCloud::OSS::PutObjectRequest request((std::string(bucket)), (std::string(key)), content);
  auto outcome = oss_client_.PutObject(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  return Status();
}

Status AliyunOssObjectStore::get_object(const std::string_view &bucket,
                                        const std::string_view &key,
                                        std::string &input) {
  AlibabaCloud::OSS::GetObjectRequest request((std::string(bucket)), (std::string(key)));

  AlibabaCloud::OSS::GetObjectOutcome outcome;

  outcome = oss_client_.GetObject(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  std::ostringstream oss;
  oss << outcome.result().Content()->rdbuf();
  if (!oss) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  input = oss.str();
  return Status();
}

Status AliyunOssObjectStore::get_object(const std::string_view &bucket,
                                        const std::string_view &key, size_t off,
                                        size_t len, std::string &body) {
  AlibabaCloud::OSS::GetObjectRequest request((std::string(bucket)), (std::string(key)));
  AlibabaCloud::OSS::GetObjectOutcome outcome = oss_client_.GetObject(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }

  std::ostringstream oss;
  oss << outcome.result().Content()->rdbuf();
  if (!oss) {
    return Status(Errors::SE_IO_ERROR, 0,
                  "unable to read data from response stream");
  }

  body = oss.str();
  if (off >= body.length()) {
    return Status(Errors::CLOUD_PROVIDER_UNRECOVERABLE_ERROR, 0, "Unable to parse ExceptionName: InvalidRange Message: The requested range is not satisfiable");
  }
  body = body.substr(off, len);
  return Status();
}

Status AliyunOssObjectStore::get_object_meta(const std::string_view &bucket,
                                             const std::string_view &key,
                                             ObjectMeta &meta) {
  AlibabaCloud::OSS::HeadObjectRequest request((std::string(bucket)), (std::string(key)));
  AlibabaCloud::OSS::ObjectMetaDataOutcome outcome = oss_client_.HeadObject(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  meta.key = key;
  meta.last_modified = convertTimeStr2Int64(outcome.result().LastModified());
  return Status();
}

Status AliyunOssObjectStore::list_object(const std::string_view &bucket,
                                         const std::string_view &prefix,
                                         std::string_view &start_after,
                                         bool &finished,
                                         std::vector<ObjectMeta> &objects) {
  AlibabaCloud::OSS::ListObjectsRequest request((std::string(bucket)));
  request.setPrefix(std::string(prefix));
  if (!start_after.empty()) {
    request.setMarker(std::string(start_after));
  }
  request.setMaxKeys(LIST_MAX_KEYS);
  auto outcome = oss_client_.ListObjects(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }

  const auto & aliyun_objects = outcome.result().ObjectSummarys();
  for (const auto &obj : aliyun_objects) {
    ObjectMeta meta;
    meta.key = obj.Key();
    meta.last_modified = convertTimeStr2Int64(obj.LastModified());
    meta.size = obj.Size();
    objects.push_back(meta);
  }
  finished = !outcome.result().IsTruncated();
  if (finished) {
    start_after = "";
  } else {
    if (!aliyun_objects.empty()) {
      start_after = outcome.result().NextMarker();
    } else {
      Errors err_type = Errors::CLOUD_PROVIDER_UNRECOVERABLE_ERROR;
      return Status(err_type, 0,
                    "list object returned empty objects but should not");
    }
  }
  return Status();
}

Status AliyunOssObjectStore::delete_object(const std::string_view &bucket,
                                           const std::string_view &key) {
  AlibabaCloud::OSS::DeleteObjectRequest request((std::string(bucket)), (std::string(key)));
  request.setVersionId("null");
  auto outcome = oss_client_.DeleteObject(request);
  if (!outcome.isSuccess()) {
    Errors err_type = aliyun_oss_error_to_se_error(outcome.error());
    return Status(err_type, outcome.error().Code(), outcome.error().Message());
  }
  return Status();
}

void init_aliyun_api() { AlibabaCloud::OSS::InitializeSdk(); }

void shutdown_aliyun_api() { AlibabaCloud::OSS::ShutdownSdk(); }

char *get_oss_access_key_id() {
  char *access_key_id = std::getenv("OSS_ACCESS_KEY_ID");
  if (access_key_id) {
    return access_key_id;
  }
  access_key_id = std::getenv("ACCESS_KEY_ID");
  if (access_key_id) {
    return access_key_id;
  }
  return nullptr;
}

char *get_oss_access_secret_key() {
  char *access_secret_key = std::getenv("OSS_ACCESS_KEY_SECRET");
  if (access_secret_key) {
    return access_secret_key;
  }
  access_secret_key = std::getenv("SECRET_ACCESS_KEY");
  if (access_secret_key) {
    return access_secret_key;
  }
  return nullptr;
}

AliyunOssObjectStore* create_aliyun_oss_objstore_helper(const std::string_view region,
                                                        const std::string_view *endpoint,
                                                        const std::string_view bucket_dir) {
  if (endpoint == nullptr) {
    return nullptr;
  }
  AlibabaCloud::OSS::ClientConfiguration conf;
  char *access_key_id = get_oss_access_key_id();
  char *access_secret_key = get_oss_access_secret_key();
  
  if (access_key_id && access_secret_key) {
    std::string access_key_id_str(access_key_id);
    std::string access_secret_key_str(access_secret_key);
    AlibabaCloud::OSS::OssClient client(std::string(*endpoint), access_key_id_str, access_secret_key_str, conf);
    return new AliyunOssObjectStore(region, std::move(client), bucket_dir);
  }
  return nullptr;
}

AliyunOssObjectStore *create_aliyun_oss_objstore(
    const std::string_view region, const std::string_view *endpoint) {
  return create_aliyun_oss_objstore_helper(region, endpoint, "");
}

AliyunOssObjectStore *create_aliyun_oss_objstore_for_test(
    const std::string_view region, const std::string_view *endpoint,
    const std::string_view bucket_dir) {
  return create_aliyun_oss_objstore_helper(region, endpoint, bucket_dir);
}

void destroy_aliyun_oss_objstore(AliyunOssObjectStore *oss_obj_store) {
  if (oss_obj_store) {
    delete oss_obj_store;
  }
  return;
}

}  // namespace objstore