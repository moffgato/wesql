﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/swf/SWF_EXPORTS.h>
#include <aws/swf/SWFRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <utility>

namespace Aws
{
namespace SWF
{
namespace Model
{

  /**
   */
  class RespondActivityTaskFailedRequest : public SWFRequest
  {
  public:
    AWS_SWF_API RespondActivityTaskFailedRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "RespondActivityTaskFailed"; }

    AWS_SWF_API Aws::String SerializePayload() const override;

    AWS_SWF_API Aws::Http::HeaderValueCollection GetRequestSpecificHeaders() const override;


    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline const Aws::String& GetTaskToken() const{ return m_taskToken; }

    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline bool TaskTokenHasBeenSet() const { return m_taskTokenHasBeenSet; }

    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline void SetTaskToken(const Aws::String& value) { m_taskTokenHasBeenSet = true; m_taskToken = value; }

    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline void SetTaskToken(Aws::String&& value) { m_taskTokenHasBeenSet = true; m_taskToken = std::move(value); }

    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline void SetTaskToken(const char* value) { m_taskTokenHasBeenSet = true; m_taskToken.assign(value); }

    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline RespondActivityTaskFailedRequest& WithTaskToken(const Aws::String& value) { SetTaskToken(value); return *this;}

    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline RespondActivityTaskFailedRequest& WithTaskToken(Aws::String&& value) { SetTaskToken(std::move(value)); return *this;}

    /**
     * <p>The <code>taskToken</code> of the <a>ActivityTask</a>.</p>  <p>
     * <code>taskToken</code> is generated by the service and should be treated as an
     * opaque value. If the task is passed to another process, its
     * <code>taskToken</code> must also be passed. This enables it to provide its
     * progress and respond with results.</p> 
     */
    inline RespondActivityTaskFailedRequest& WithTaskToken(const char* value) { SetTaskToken(value); return *this;}


    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline const Aws::String& GetReason() const{ return m_reason; }

    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline bool ReasonHasBeenSet() const { return m_reasonHasBeenSet; }

    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline void SetReason(const Aws::String& value) { m_reasonHasBeenSet = true; m_reason = value; }

    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline void SetReason(Aws::String&& value) { m_reasonHasBeenSet = true; m_reason = std::move(value); }

    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline void SetReason(const char* value) { m_reasonHasBeenSet = true; m_reason.assign(value); }

    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline RespondActivityTaskFailedRequest& WithReason(const Aws::String& value) { SetReason(value); return *this;}

    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline RespondActivityTaskFailedRequest& WithReason(Aws::String&& value) { SetReason(std::move(value)); return *this;}

    /**
     * <p>Description of the error that may assist in diagnostics.</p>
     */
    inline RespondActivityTaskFailedRequest& WithReason(const char* value) { SetReason(value); return *this;}


    /**
     * <p> Detailed information about the failure.</p>
     */
    inline const Aws::String& GetDetails() const{ return m_details; }

    /**
     * <p> Detailed information about the failure.</p>
     */
    inline bool DetailsHasBeenSet() const { return m_detailsHasBeenSet; }

    /**
     * <p> Detailed information about the failure.</p>
     */
    inline void SetDetails(const Aws::String& value) { m_detailsHasBeenSet = true; m_details = value; }

    /**
     * <p> Detailed information about the failure.</p>
     */
    inline void SetDetails(Aws::String&& value) { m_detailsHasBeenSet = true; m_details = std::move(value); }

    /**
     * <p> Detailed information about the failure.</p>
     */
    inline void SetDetails(const char* value) { m_detailsHasBeenSet = true; m_details.assign(value); }

    /**
     * <p> Detailed information about the failure.</p>
     */
    inline RespondActivityTaskFailedRequest& WithDetails(const Aws::String& value) { SetDetails(value); return *this;}

    /**
     * <p> Detailed information about the failure.</p>
     */
    inline RespondActivityTaskFailedRequest& WithDetails(Aws::String&& value) { SetDetails(std::move(value)); return *this;}

    /**
     * <p> Detailed information about the failure.</p>
     */
    inline RespondActivityTaskFailedRequest& WithDetails(const char* value) { SetDetails(value); return *this;}

  private:

    Aws::String m_taskToken;
    bool m_taskTokenHasBeenSet = false;

    Aws::String m_reason;
    bool m_reasonHasBeenSet = false;

    Aws::String m_details;
    bool m_detailsHasBeenSet = false;
  };

} // namespace Model
} // namespace SWF
} // namespace Aws