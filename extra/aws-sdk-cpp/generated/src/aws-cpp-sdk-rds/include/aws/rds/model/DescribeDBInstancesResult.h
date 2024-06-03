﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/rds/RDS_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/memory/stl/AWSVector.h>
#include <aws/rds/model/ResponseMetadata.h>
#include <aws/rds/model/DBInstance.h>
#include <utility>

namespace Aws
{
template<typename RESULT_TYPE>
class AmazonWebServiceResult;

namespace Utils
{
namespace Xml
{
  class XmlDocument;
} // namespace Xml
} // namespace Utils
namespace RDS
{
namespace Model
{
  /**
   * <p>Contains the result of a successful invocation of the
   * <code>DescribeDBInstances</code> action.</p><p><h3>See Also:</h3>   <a
   * href="http://docs.aws.amazon.com/goto/WebAPI/rds-2014-10-31/DBInstanceMessage">AWS
   * API Reference</a></p>
   */
  class DescribeDBInstancesResult
  {
  public:
    AWS_RDS_API DescribeDBInstancesResult();
    AWS_RDS_API DescribeDBInstancesResult(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);
    AWS_RDS_API DescribeDBInstancesResult& operator=(const Aws::AmazonWebServiceResult<Aws::Utils::Xml::XmlDocument>& result);


    /**
     * <p>An optional pagination token provided by a previous request. If this
     * parameter is specified, the response includes only records beyond the marker, up
     * to the value specified by <code>MaxRecords</code> .</p>
     */
    inline const Aws::String& GetMarker() const{ return m_marker; }

    /**
     * <p>An optional pagination token provided by a previous request. If this
     * parameter is specified, the response includes only records beyond the marker, up
     * to the value specified by <code>MaxRecords</code> .</p>
     */
    inline void SetMarker(const Aws::String& value) { m_marker = value; }

    /**
     * <p>An optional pagination token provided by a previous request. If this
     * parameter is specified, the response includes only records beyond the marker, up
     * to the value specified by <code>MaxRecords</code> .</p>
     */
    inline void SetMarker(Aws::String&& value) { m_marker = std::move(value); }

    /**
     * <p>An optional pagination token provided by a previous request. If this
     * parameter is specified, the response includes only records beyond the marker, up
     * to the value specified by <code>MaxRecords</code> .</p>
     */
    inline void SetMarker(const char* value) { m_marker.assign(value); }

    /**
     * <p>An optional pagination token provided by a previous request. If this
     * parameter is specified, the response includes only records beyond the marker, up
     * to the value specified by <code>MaxRecords</code> .</p>
     */
    inline DescribeDBInstancesResult& WithMarker(const Aws::String& value) { SetMarker(value); return *this;}

    /**
     * <p>An optional pagination token provided by a previous request. If this
     * parameter is specified, the response includes only records beyond the marker, up
     * to the value specified by <code>MaxRecords</code> .</p>
     */
    inline DescribeDBInstancesResult& WithMarker(Aws::String&& value) { SetMarker(std::move(value)); return *this;}

    /**
     * <p>An optional pagination token provided by a previous request. If this
     * parameter is specified, the response includes only records beyond the marker, up
     * to the value specified by <code>MaxRecords</code> .</p>
     */
    inline DescribeDBInstancesResult& WithMarker(const char* value) { SetMarker(value); return *this;}


    /**
     * <p>A list of <code>DBInstance</code> instances.</p>
     */
    inline const Aws::Vector<DBInstance>& GetDBInstances() const{ return m_dBInstances; }

    /**
     * <p>A list of <code>DBInstance</code> instances.</p>
     */
    inline void SetDBInstances(const Aws::Vector<DBInstance>& value) { m_dBInstances = value; }

    /**
     * <p>A list of <code>DBInstance</code> instances.</p>
     */
    inline void SetDBInstances(Aws::Vector<DBInstance>&& value) { m_dBInstances = std::move(value); }

    /**
     * <p>A list of <code>DBInstance</code> instances.</p>
     */
    inline DescribeDBInstancesResult& WithDBInstances(const Aws::Vector<DBInstance>& value) { SetDBInstances(value); return *this;}

    /**
     * <p>A list of <code>DBInstance</code> instances.</p>
     */
    inline DescribeDBInstancesResult& WithDBInstances(Aws::Vector<DBInstance>&& value) { SetDBInstances(std::move(value)); return *this;}

    /**
     * <p>A list of <code>DBInstance</code> instances.</p>
     */
    inline DescribeDBInstancesResult& AddDBInstances(const DBInstance& value) { m_dBInstances.push_back(value); return *this; }

    /**
     * <p>A list of <code>DBInstance</code> instances.</p>
     */
    inline DescribeDBInstancesResult& AddDBInstances(DBInstance&& value) { m_dBInstances.push_back(std::move(value)); return *this; }


    
    inline const ResponseMetadata& GetResponseMetadata() const{ return m_responseMetadata; }

    
    inline void SetResponseMetadata(const ResponseMetadata& value) { m_responseMetadata = value; }

    
    inline void SetResponseMetadata(ResponseMetadata&& value) { m_responseMetadata = std::move(value); }

    
    inline DescribeDBInstancesResult& WithResponseMetadata(const ResponseMetadata& value) { SetResponseMetadata(value); return *this;}

    
    inline DescribeDBInstancesResult& WithResponseMetadata(ResponseMetadata&& value) { SetResponseMetadata(std::move(value)); return *this;}

  private:

    Aws::String m_marker;

    Aws::Vector<DBInstance> m_dBInstances;

    ResponseMetadata m_responseMetadata;
  };

} // namespace Model
} // namespace RDS
} // namespace Aws