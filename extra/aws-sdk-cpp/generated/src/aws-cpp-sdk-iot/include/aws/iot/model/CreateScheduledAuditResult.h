﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/iot/IoT_EXPORTS.h>
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
namespace IoT
{
namespace Model
{
  class CreateScheduledAuditResult
  {
  public:
    AWS_IOT_API CreateScheduledAuditResult();
    AWS_IOT_API CreateScheduledAuditResult(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);
    AWS_IOT_API CreateScheduledAuditResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Json::JsonValue>& result);


    /**
     * <p>The ARN of the scheduled audit.</p>
     */
    inline const Aws::String& GetScheduledAuditArn() const{ return m_scheduledAuditArn; }

    /**
     * <p>The ARN of the scheduled audit.</p>
     */
    inline void SetScheduledAuditArn(const Aws::String& value) { m_scheduledAuditArn = value; }

    /**
     * <p>The ARN of the scheduled audit.</p>
     */
    inline void SetScheduledAuditArn(Aws::String&& value) { m_scheduledAuditArn = std::move(value); }

    /**
     * <p>The ARN of the scheduled audit.</p>
     */
    inline void SetScheduledAuditArn(const char* value) { m_scheduledAuditArn.assign(value); }

    /**
     * <p>The ARN of the scheduled audit.</p>
     */
    inline CreateScheduledAuditResult& WithScheduledAuditArn(const Aws::String& value) { SetScheduledAuditArn(value); return *this;}

    /**
     * <p>The ARN of the scheduled audit.</p>
     */
    inline CreateScheduledAuditResult& WithScheduledAuditArn(Aws::String&& value) { SetScheduledAuditArn(std::move(value)); return *this;}

    /**
     * <p>The ARN of the scheduled audit.</p>
     */
    inline CreateScheduledAuditResult& WithScheduledAuditArn(const char* value) { SetScheduledAuditArn(value); return *this;}


    
    inline const Aws::String& GetRequestId() const{ return m_requestId; }

    
    inline void SetRequestId(const Aws::String& value) { m_requestId = value; }

    
    inline void SetRequestId(Aws::String&& value) { m_requestId = std::move(value); }

    
    inline void SetRequestId(const char* value) { m_requestId.assign(value); }

    
    inline CreateScheduledAuditResult& WithRequestId(const Aws::String& value) { SetRequestId(value); return *this;}

    
    inline CreateScheduledAuditResult& WithRequestId(Aws::String&& value) { SetRequestId(std::move(value)); return *this;}

    
    inline CreateScheduledAuditResult& WithRequestId(const char* value) { SetRequestId(value); return *this;}

  private:

    Aws::String m_scheduledAuditArn;

    Aws::String m_requestId;
  };

} // namespace Model
} // namespace IoT
} // namespace Aws