﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/ivschat/Ivschat_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <utility>

namespace Aws
{
template<typename RESULT_TYPE>
class AmazonWebServiceResult;

namespace Utils
{
namespace Json
{
  class JsonValue;
} // namespace Json
} // namespace Utils
namespace ivschat
{
namespace Model
{
  class DeleteMessageResult
  {
  public:
    AWS_IVSCHAT_API DeleteMessageResult();
    AWS_IVSCHAT_API DeleteMessageResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    AWS_IVSCHAT_API DeleteMessageResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    /**
     * <p>Operation identifier, generated by Amazon IVS Chat.</p>
     */
    inline const Aws::String& GetId() const{ return m_id; }

    /**
     * <p>Operation identifier, generated by Amazon IVS Chat.</p>
     */
    inline void SetId(const Aws::String& value) { m_id = value; }

    /**
     * <p>Operation identifier, generated by Amazon IVS Chat.</p>
     */
    inline void SetId(Aws::String&& value) { m_id = std::move(value); }

    /**
     * <p>Operation identifier, generated by Amazon IVS Chat.</p>
     */
    inline void SetId(const char* value) { m_id.assign(value); }

    /**
     * <p>Operation identifier, generated by Amazon IVS Chat.</p>
     */
    inline DeleteMessageResult& WithId(const Aws::String& value) { SetId(value); return *this;}

    /**
     * <p>Operation identifier, generated by Amazon IVS Chat.</p>
     */
    inline DeleteMessageResult& WithId(Aws::String&& value) { SetId(std::move(value)); return *this;}

    /**
     * <p>Operation identifier, generated by Amazon IVS Chat.</p>
     */
    inline DeleteMessageResult& WithId(const char* value) { SetId(value); return *this;}


    
    inline const Aws::String& GetRequestId() const{ return m_requestId; }

    
    inline void SetRequestId(const Aws::String& value) { m_requestId = value; }

    
    inline void SetRequestId(Aws::String&& value) { m_requestId = std::move(value); }

    
    inline void SetRequestId(const char* value) { m_requestId.assign(value); }

    
    inline DeleteMessageResult& WithRequestId(const Aws::String& value) { SetRequestId(value); return *this;}

    
    inline DeleteMessageResult& WithRequestId(Aws::String&& value) { SetRequestId(std::move(value)); return *this;}

    
    inline DeleteMessageResult& WithRequestId(const char* value) { SetRequestId(value); return *this;}

  private:

    Aws::String m_id;

    Aws::String m_requestId;
  };

} // namespace Model
} // namespace ivschat
} // namespace Aws