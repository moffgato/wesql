﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/connect/Connect_EXPORTS.h>
#include <aws/connect/ConnectRequest.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/connect/model/UserProficiencyDisassociate.h>
#include <utility>

namespace Aws
{
namespace Connect
{
namespace Model
{

  /**
   */
  class DisassociateUserProficienciesRequest : public ConnectRequest
  {
  public:
    AWS_CONNECT_API DisassociateUserProficienciesRequest();

    // Service request name is the Operation name which will send this request out,
    // each operation should has unique request name, so that we can get operation's name from this request.
    // Note: this is not true for response, multiple operations may have the same response name,
    // so we can not get operation's name from response.
    inline virtual const char* GetServiceRequestName() const override { return "DisassociateUserProficiencies"; }

    AWS_CONNECT_API Aws::String SerializePayload() const override;


    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline const Aws::String& GetInstanceId() const{ return m_instanceId; }

    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline bool InstanceIdHasBeenSet() const { return m_instanceIdHasBeenSet; }

    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline void SetInstanceId(const Aws::String& value) { m_instanceIdHasBeenSet = true; m_instanceId = value; }

    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline void SetInstanceId(Aws::String&& value) { m_instanceIdHasBeenSet = true; m_instanceId = std::move(value); }

    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline void SetInstanceId(const char* value) { m_instanceIdHasBeenSet = true; m_instanceId.assign(value); }

    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline DisassociateUserProficienciesRequest& WithInstanceId(const Aws::String& value) { SetInstanceId(value); return *this;}

    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline DisassociateUserProficienciesRequest& WithInstanceId(Aws::String&& value) { SetInstanceId(std::move(value)); return *this;}

    /**
     * <p>The identifier of the Amazon Connect instance. You can find the instance ID
     * in the Amazon Resource Name (ARN) of the instance.</p>
     */
    inline DisassociateUserProficienciesRequest& WithInstanceId(const char* value) { SetInstanceId(value); return *this;}


    /**
     * <p>The identifier of the user account.</p>
     */
    inline const Aws::String& GetUserId() const{ return m_userId; }

    /**
     * <p>The identifier of the user account.</p>
     */
    inline bool UserIdHasBeenSet() const { return m_userIdHasBeenSet; }

    /**
     * <p>The identifier of the user account.</p>
     */
    inline void SetUserId(const Aws::String& value) { m_userIdHasBeenSet = true; m_userId = value; }

    /**
     * <p>The identifier of the user account.</p>
     */
    inline void SetUserId(Aws::String&& value) { m_userIdHasBeenSet = true; m_userId = std::move(value); }

    /**
     * <p>The identifier of the user account.</p>
     */
    inline void SetUserId(const char* value) { m_userIdHasBeenSet = true; m_userId.assign(value); }

    /**
     * <p>The identifier of the user account.</p>
     */
    inline DisassociateUserProficienciesRequest& WithUserId(const Aws::String& value) { SetUserId(value); return *this;}

    /**
     * <p>The identifier of the user account.</p>
     */
    inline DisassociateUserProficienciesRequest& WithUserId(Aws::String&& value) { SetUserId(std::move(value)); return *this;}

    /**
     * <p>The identifier of the user account.</p>
     */
    inline DisassociateUserProficienciesRequest& WithUserId(const char* value) { SetUserId(value); return *this;}


    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline const Aws::Vector<UserProficiencyDisassociate>& GetUserProficiencies() const{ return m_userProficiencies; }

    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline bool UserProficienciesHasBeenSet() const { return m_userProficienciesHasBeenSet; }

    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline void SetUserProficiencies(const Aws::Vector<UserProficiencyDisassociate>& value) { m_userProficienciesHasBeenSet = true; m_userProficiencies = value; }

    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline void SetUserProficiencies(Aws::Vector<UserProficiencyDisassociate>&& value) { m_userProficienciesHasBeenSet = true; m_userProficiencies = std::move(value); }

    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline DisassociateUserProficienciesRequest& WithUserProficiencies(const Aws::Vector<UserProficiencyDisassociate>& value) { SetUserProficiencies(value); return *this;}

    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline DisassociateUserProficienciesRequest& WithUserProficiencies(Aws::Vector<UserProficiencyDisassociate>&& value) { SetUserProficiencies(std::move(value)); return *this;}

    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline DisassociateUserProficienciesRequest& AddUserProficiencies(const UserProficiencyDisassociate& value) { m_userProficienciesHasBeenSet = true; m_userProficiencies.push_back(value); return *this; }

    /**
     * <p>The proficiencies to disassociate from the user.</p>
     */
    inline DisassociateUserProficienciesRequest& AddUserProficiencies(UserProficiencyDisassociate&& value) { m_userProficienciesHasBeenSet = true; m_userProficiencies.push_back(std::move(value)); return *this; }

  private:

    Aws::String m_instanceId;
    bool m_instanceIdHasBeenSet = false;

    Aws::String m_userId;
    bool m_userIdHasBeenSet = false;

    Aws::Vector<UserProficiencyDisassociate> m_userProficiencies;
    bool m_userProficienciesHasBeenSet = false;
  };

} // namespace Model
} // namespace Connect
} // namespace Aws