﻿/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#include <aws/cloudfront-keyvaluestore/CloudFrontKeyValueStoreEndpointRules.h>
#include <aws/core/utils/memory/stl/AWSArray.h>

namespace Aws
{
namespace CloudFrontKeyValueStore
{
const size_t CloudFrontKeyValueStoreEndpointRules::RulesBlobStrLen = 4754;
const size_t CloudFrontKeyValueStoreEndpointRules::RulesBlobSize = 4755;

using RulesBlobT = Aws::Array<const char, CloudFrontKeyValueStoreEndpointRules::RulesBlobSize>;
static constexpr RulesBlobT RulesBlob = {{
'{','"','v','e','r','s','i','o','n','"',':','"','1','.','0','"',',','"','p','a','r','a','m','e','t',
'e','r','s','"',':','{','"','K','v','s','A','R','N','"',':','{','"','r','e','q','u','i','r','e','d',
'"',':','f','a','l','s','e',',','"','d','o','c','u','m','e','n','t','a','t','i','o','n','"',':','"',
'T','h','e',' ','A','R','N',' ','o','f',' ','t','h','e',' ','K','e','y',' ','V','a','l','u','e',' ',
'S','t','o','r','e','"',',','"','t','y','p','e','"',':','"','S','t','r','i','n','g','"','}',',','"',
'R','e','g','i','o','n','"',':','{','"','b','u','i','l','t','I','n','"',':','"','A','W','S',':',':',
'R','e','g','i','o','n','"',',','"','r','e','q','u','i','r','e','d','"',':','f','a','l','s','e',',',
'"','d','o','c','u','m','e','n','t','a','t','i','o','n','"',':','"','T','h','e',' ','A','W','S',' ',
'r','e','g','i','o','n',' ','u','s','e','d',' ','t','o',' ','d','i','s','p','a','t','c','h',' ','t',
'h','e',' ','r','e','q','u','e','s','t','.','"',',','"','t','y','p','e','"',':','"','S','t','r','i',
'n','g','"','}',',','"','U','s','e','F','I','P','S','"',':','{','"','b','u','i','l','t','I','n','"',
':','"','A','W','S',':',':','U','s','e','F','I','P','S','"',',','"','r','e','q','u','i','r','e','d',
'"',':','t','r','u','e',',','"','d','e','f','a','u','l','t','"',':','f','a','l','s','e',',','"','d',
'o','c','u','m','e','n','t','a','t','i','o','n','"',':','"','W','h','e','n',' ','t','r','u','e',',',
' ','s','e','n','d',' ','t','h','i','s',' ','r','e','q','u','e','s','t',' ','t','o',' ','t','h','e',
' ','F','I','P','S','-','c','o','m','p','l','i','a','n','t',' ','r','e','g','i','o','n','a','l',' ',
'e','n','d','p','o','i','n','t','.',' ','I','f',' ','t','h','e',' ','c','o','n','f','i','g','u','r',
'e','d',' ','e','n','d','p','o','i','n','t',' ','d','o','e','s',' ','n','o','t',' ','h','a','v','e',
' ','a',' ','F','I','P','S',' ','c','o','m','p','l','i','a','n','t',' ','e','n','d','p','o','i','n',
't',',',' ','d','i','s','p','a','t','c','h','i','n','g',' ','t','h','e',' ','r','e','q','u','e','s',
't',' ','w','i','l','l',' ','r','e','t','u','r','n',' ','a','n',' ','e','r','r','o','r','.','"',',',
'"','t','y','p','e','"',':','"','B','o','o','l','e','a','n','"','}',',','"','E','n','d','p','o','i',
'n','t','"',':','{','"','b','u','i','l','t','I','n','"',':','"','S','D','K',':',':','E','n','d','p',
'o','i','n','t','"',',','"','r','e','q','u','i','r','e','d','"',':','f','a','l','s','e',',','"','d',
'o','c','u','m','e','n','t','a','t','i','o','n','"',':','"','O','v','e','r','r','i','d','e',' ','t',
'h','e',' ','e','n','d','p','o','i','n','t',' ','u','s','e','d',' ','t','o',' ','s','e','n','d',' ',
't','h','i','s',' ','r','e','q','u','e','s','t','"',',','"','t','y','p','e','"',':','"','S','t','r',
'i','n','g','"','}','}',',','"','r','u','l','e','s','"',':','[','{','"','c','o','n','d','i','t','i',
'o','n','s','"',':','[','{','"','f','n','"',':','"','b','o','o','l','e','a','n','E','q','u','a','l',
's','"',',','"','a','r','g','v','"',':','[','{','"','r','e','f','"',':','"','U','s','e','F','I','P',
'S','"','}',',','f','a','l','s','e',']','}',']',',','"','r','u','l','e','s','"',':','[','{','"','c',
'o','n','d','i','t','i','o','n','s','"',':','[','{','"','f','n','"',':','"','i','s','S','e','t','"',
',','"','a','r','g','v','"',':','[','{','"','r','e','f','"',':','"','K','v','s','A','R','N','"','}',
']','}',']',',','"','r','u','l','e','s','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s',
'"',':','[','{','"','f','n','"',':','"','a','w','s','.','p','a','r','s','e','A','r','n','"',',','"',
'a','r','g','v','"',':','[','{','"','r','e','f','"',':','"','K','v','s','A','R','N','"','}',']',',',
'"','a','s','s','i','g','n','"',':','"','p','a','r','s','e','d','A','r','n','"','}',']',',','"','r',
'u','l','e','s','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[','{','"','f',
'n','"',':','"','s','t','r','i','n','g','E','q','u','a','l','s','"',',','"','a','r','g','v','"',':',
'[','{','"','f','n','"',':','"','g','e','t','A','t','t','r','"',',','"','a','r','g','v','"',':','[',
'{','"','r','e','f','"',':','"','p','a','r','s','e','d','A','r','n','"','}',',','"','s','e','r','v',
'i','c','e','"',']','}',',','"','c','l','o','u','d','f','r','o','n','t','"',']','}',']',',','"','r',
'u','l','e','s','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[','{','"','f',
'n','"',':','"','s','t','r','i','n','g','E','q','u','a','l','s','"',',','"','a','r','g','v','"',':',
'[','{','"','f','n','"',':','"','g','e','t','A','t','t','r','"',',','"','a','r','g','v','"',':','[',
'{','"','r','e','f','"',':','"','p','a','r','s','e','d','A','r','n','"','}',',','"','r','e','g','i',
'o','n','"',']','}',',','"','"',']','}',']',',','"','r','u','l','e','s','"',':','[','{','"','c','o',
'n','d','i','t','i','o','n','s','"',':','[','{','"','f','n','"',':','"','g','e','t','A','t','t','r',
'"',',','"','a','r','g','v','"',':','[','{','"','r','e','f','"',':','"','p','a','r','s','e','d','A',
'r','n','"','}',',','"','r','e','s','o','u','r','c','e','I','d','[','0',']','"',']',',','"','a','s',
's','i','g','n','"',':','"','a','r','n','T','y','p','e','"','}',']',',','"','r','u','l','e','s','"',
':','[','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[','{','"','f','n','"',':','"','n',
'o','t','"',',','"','a','r','g','v','"',':','[','{','"','f','n','"',':','"','s','t','r','i','n','g',
'E','q','u','a','l','s','"',',','"','a','r','g','v','"',':','[','{','"','r','e','f','"',':','"','a',
'r','n','T','y','p','e','"','}',',','"','"',']','}',']','}',']',',','"','r','u','l','e','s','"',':',
'[','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[','{','"','f','n','"',':','"','s','t',
'r','i','n','g','E','q','u','a','l','s','"',',','"','a','r','g','v','"',':','[','{','"','r','e','f',
'"',':','"','a','r','n','T','y','p','e','"','}',',','"','k','e','y','-','v','a','l','u','e','-','s',
't','o','r','e','"',']','}',']',',','"','r','u','l','e','s','"',':','[','{','"','c','o','n','d','i',
't','i','o','n','s','"',':','[','{','"','f','n','"',':','"','s','t','r','i','n','g','E','q','u','a',
'l','s','"',',','"','a','r','g','v','"',':','[','{','"','f','n','"',':','"','g','e','t','A','t','t',
'r','"',',','"','a','r','g','v','"',':','[','{','"','r','e','f','"',':','"','p','a','r','s','e','d',
'A','r','n','"','}',',','"','p','a','r','t','i','t','i','o','n','"',']','}',',','"','a','w','s','"',
']','}',']',',','"','r','u','l','e','s','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s',
'"',':','[','{','"','f','n','"',':','"','i','s','S','e','t','"',',','"','a','r','g','v','"',':','[',
'{','"','r','e','f','"',':','"','R','e','g','i','o','n','"','}',']','}',']',',','"','r','u','l','e',
's','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[','{','"','f','n','"',':',
'"','a','w','s','.','p','a','r','t','i','t','i','o','n','"',',','"','a','r','g','v','"',':','[','{',
'"','r','e','f','"',':','"','R','e','g','i','o','n','"','}',']',',','"','a','s','s','i','g','n','"',
':','"','p','a','r','t','i','t','i','o','n','R','e','s','u','l','t','"','}',']',',','"','r','u','l',
'e','s','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[','{','"','f','n','"',
':','"','s','t','r','i','n','g','E','q','u','a','l','s','"',',','"','a','r','g','v','"',':','[','{',
'"','f','n','"',':','"','g','e','t','A','t','t','r','"',',','"','a','r','g','v','"',':','[','{','"',
'r','e','f','"',':','"','p','a','r','t','i','t','i','o','n','R','e','s','u','l','t','"','}',',','"',
'n','a','m','e','"',']','}',',','"','{','p','a','r','s','e','d','A','r','n','#','p','a','r','t','i',
't','i','o','n','}','"',']','}',']',',','"','r','u','l','e','s','"',':','[','{','"','c','o','n','d',
'i','t','i','o','n','s','"',':','[','{','"','f','n','"',':','"','i','s','S','e','t','"',',','"','a',
'r','g','v','"',':','[','{','"','r','e','f','"',':','"','E','n','d','p','o','i','n','t','"','}',']',
'}',']',',','"','r','u','l','e','s','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s','"',
':','[','{','"','f','n','"',':','"','p','a','r','s','e','U','R','L','"',',','"','a','r','g','v','"',
':','[','{','"','r','e','f','"',':','"','E','n','d','p','o','i','n','t','"','}',']',',','"','a','s',
's','i','g','n','"',':','"','u','r','l','"','}',']',',','"','r','u','l','e','s','"',':','[','{','"',
'c','o','n','d','i','t','i','o','n','s','"',':','[',']',',','"','e','n','d','p','o','i','n','t','"',
':','{','"','u','r','l','"',':','"','{','u','r','l','#','s','c','h','e','m','e','}',':','/','/','{',
'p','a','r','s','e','d','A','r','n','#','a','c','c','o','u','n','t','I','d','}','.','{','u','r','l',
'#','a','u','t','h','o','r','i','t','y','}','{','u','r','l','#','p','a','t','h','}','"',',','"','p',
'r','o','p','e','r','t','i','e','s','"',':','{','"','a','u','t','h','S','c','h','e','m','e','s','"',
':','[','{','"','n','a','m','e','"',':','"','s','i','g','v','4','a','"',',','"','s','i','g','n','i',
'n','g','N','a','m','e','"',':','"','c','l','o','u','d','f','r','o','n','t','-','k','e','y','v','a',
'l','u','e','s','t','o','r','e','"',',','"','s','i','g','n','i','n','g','R','e','g','i','o','n','S',
'e','t','"',':','[','"','*','"',']','}',']','}',',','"','h','e','a','d','e','r','s','"',':','{','}',
'}',',','"','t','y','p','e','"',':','"','e','n','d','p','o','i','n','t','"','}',']',',','"','t','y',
'p','e','"',':','"','t','r','e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',
':','[',']',',','"','e','r','r','o','r','"',':','"','P','r','o','v','i','d','e','d',' ','e','n','d',
'p','o','i','n','t',' ','i','s',' ','n','o','t',' ','a',' ','v','a','l','i','d',' ','U','R','L','"',
',','"','t','y','p','e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',':',
'"','t','r','e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',',',
'"','e','n','d','p','o','i','n','t','"',':','{','"','u','r','l','"',':','"','h','t','t','p','s',':',
'/','/','{','p','a','r','s','e','d','A','r','n','#','a','c','c','o','u','n','t','I','d','}','.','c',
'l','o','u','d','f','r','o','n','t','-','k','v','s','.','g','l','o','b','a','l','.','a','p','i','.',
'a','w','s','"',',','"','p','r','o','p','e','r','t','i','e','s','"',':','{','"','a','u','t','h','S',
'c','h','e','m','e','s','"',':','[','{','"','n','a','m','e','"',':','"','s','i','g','v','4','a','"',
',','"','s','i','g','n','i','n','g','N','a','m','e','"',':','"','c','l','o','u','d','f','r','o','n',
't','-','k','e','y','v','a','l','u','e','s','t','o','r','e','"',',','"','s','i','g','n','i','n','g',
'R','e','g','i','o','n','S','e','t','"',':','[','"','*','"',']','}',']','}',',','"','h','e','a','d',
'e','r','s','"',':','{','}','}',',','"','t','y','p','e','"',':','"','e','n','d','p','o','i','n','t',
'"','}',']',',','"','t','y','p','e','"',':','"','t','r','e','e','"','}',',','{','"','c','o','n','d',
'i','t','i','o','n','s','"',':','[',']',',','"','e','r','r','o','r','"',':','"','C','l','i','e','n',
't',' ','w','a','s',' ','c','o','n','f','i','g','u','r','e','d',' ','f','o','r',' ','p','a','r','t',
'i','t','i','o','n',' ','`','{','p','a','r','t','i','t','i','o','n','R','e','s','u','l','t','#','n',
'a','m','e','}','`',' ','b','u','t',' ','K','v','s',' ','A','R','N',' ','h','a','s',' ','`','{','p',
'a','r','s','e','d','A','r','n','#','p','a','r','t','i','t','i','o','n','}','`','"',',','"','t','y',
'p','e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',':','"','t','r','e',
'e','"','}',']',',','"','t','y','p','e','"',':','"','t','r','e','e','"','}',',','{','"','c','o','n',
'd','i','t','i','o','n','s','"',':','[','{','"','f','n','"',':','"','i','s','S','e','t','"',',','"',
'a','r','g','v','"',':','[','{','"','r','e','f','"',':','"','E','n','d','p','o','i','n','t','"','}',
']','}',']',',','"','r','u','l','e','s','"',':','[','{','"','c','o','n','d','i','t','i','o','n','s',
'"',':','[','{','"','f','n','"',':','"','p','a','r','s','e','U','R','L','"',',','"','a','r','g','v',
'"',':','[','{','"','r','e','f','"',':','"','E','n','d','p','o','i','n','t','"','}',']',',','"','a',
's','s','i','g','n','"',':','"','u','r','l','"','}',']',',','"','r','u','l','e','s','"',':','[','{',
'"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',',','"','e','n','d','p','o','i','n','t',
'"',':','{','"','u','r','l','"',':','"','{','u','r','l','#','s','c','h','e','m','e','}',':','/','/',
'{','p','a','r','s','e','d','A','r','n','#','a','c','c','o','u','n','t','I','d','}','.','{','u','r',
'l','#','a','u','t','h','o','r','i','t','y','}','{','u','r','l','#','p','a','t','h','}','"',',','"',
'p','r','o','p','e','r','t','i','e','s','"',':','{','"','a','u','t','h','S','c','h','e','m','e','s',
'"',':','[','{','"','n','a','m','e','"',':','"','s','i','g','v','4','a','"',',','"','s','i','g','n',
'i','n','g','N','a','m','e','"',':','"','c','l','o','u','d','f','r','o','n','t','-','k','e','y','v',
'a','l','u','e','s','t','o','r','e','"',',','"','s','i','g','n','i','n','g','R','e','g','i','o','n',
'S','e','t','"',':','[','"','*','"',']','}',']','}',',','"','h','e','a','d','e','r','s','"',':','{',
'}','}',',','"','t','y','p','e','"',':','"','e','n','d','p','o','i','n','t','"','}',']',',','"','t',
'y','p','e','"',':','"','t','r','e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s',
'"',':','[',']',',','"','e','r','r','o','r','"',':','"','P','r','o','v','i','d','e','d',' ','e','n',
'd','p','o','i','n','t',' ','i','s',' ','n','o','t',' ','a',' ','v','a','l','i','d',' ','U','R','L',
'"',',','"','t','y','p','e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',
':','"','t','r','e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',
',','"','e','n','d','p','o','i','n','t','"',':','{','"','u','r','l','"',':','"','h','t','t','p','s',
':','/','/','{','p','a','r','s','e','d','A','r','n','#','a','c','c','o','u','n','t','I','d','}','.',
'c','l','o','u','d','f','r','o','n','t','-','k','v','s','.','g','l','o','b','a','l','.','a','p','i',
'.','a','w','s','"',',','"','p','r','o','p','e','r','t','i','e','s','"',':','{','"','a','u','t','h',
'S','c','h','e','m','e','s','"',':','[','{','"','n','a','m','e','"',':','"','s','i','g','v','4','a',
'"',',','"','s','i','g','n','i','n','g','N','a','m','e','"',':','"','c','l','o','u','d','f','r','o',
'n','t','-','k','e','y','v','a','l','u','e','s','t','o','r','e','"',',','"','s','i','g','n','i','n',
'g','R','e','g','i','o','n','S','e','t','"',':','[','"','*','"',']','}',']','}',',','"','h','e','a',
'd','e','r','s','"',':','{','}','}',',','"','t','y','p','e','"',':','"','e','n','d','p','o','i','n',
't','"','}',']',',','"','t','y','p','e','"',':','"','t','r','e','e','"','}',',','{','"','c','o','n',
'd','i','t','i','o','n','s','"',':','[',']',',','"','e','r','r','o','r','"',':','"','C','l','o','u',
'd','F','r','o','n','t','-','K','e','y','V','a','l','u','e','S','t','o','r','e',' ','i','s',' ','n',
'o','t',' ','s','u','p','p','o','r','t','e','d',' ','i','n',' ','p','a','r','t','i','t','i','o','n',
' ','`','{','p','a','r','s','e','d','A','r','n','#','p','a','r','t','i','t','i','o','n','}','`','"',
',','"','t','y','p','e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',':',
'"','t','r','e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',',',
'"','e','r','r','o','r','"',':','"','A','R','N',' ','r','e','s','o','u','r','c','e',' ','t','y','p',
'e',' ','i','s',' ','i','n','v','a','l','i','d','.',' ','E','x','p','e','c','t','e','d',' ','`','k',
'e','y','-','v','a','l','u','e','-','s','t','o','r','e','`',',',' ','f','o','u','n','d',':',' ','`',
'{','a','r','n','T','y','p','e','}','`','"',',','"','t','y','p','e','"',':','"','e','r','r','o','r',
'"','}',']',',','"','t','y','p','e','"',':','"','t','r','e','e','"','}',',','{','"','c','o','n','d',
'i','t','i','o','n','s','"',':','[',']',',','"','e','r','r','o','r','"',':','"','N','o',' ','r','e',
's','o','u','r','c','e',' ','t','y','p','e',' ','f','o','u','n','d',' ','i','n',' ','t','h','e',' ',
'K','V','S',' ','A','R','N','.',' ','R','e','s','o','u','r','c','e',' ','t','y','p','e',' ','m','u',
's','t',' ','b','e',' ','`','k','e','y','-','v','a','l','u','e','-','s','t','o','r','e','`','.','"',
',','"','t','y','p','e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',':',
'"','t','r','e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',',',
'"','e','r','r','o','r','"',':','"','N','o',' ','r','e','s','o','u','r','c','e',' ','t','y','p','e',
' ','f','o','u','n','d',' ','i','n',' ','t','h','e',' ','K','V','S',' ','A','R','N','.',' ','R','e',
's','o','u','r','c','e',' ','t','y','p','e',' ','m','u','s','t',' ','b','e',' ','`','k','e','y','-',
'v','a','l','u','e','-','s','t','o','r','e','`','.','"',',','"','t','y','p','e','"',':','"','e','r',
'r','o','r','"','}',']',',','"','t','y','p','e','"',':','"','t','r','e','e','"','}',',','{','"','c',
'o','n','d','i','t','i','o','n','s','"',':','[',']',',','"','e','r','r','o','r','"',':','"','P','r',
'o','v','i','d','e','d',' ','A','R','N',' ','m','u','s','t',' ','b','e',' ','a',' ','g','l','o','b',
'a','l',' ','r','e','s','o','u','r','c','e',' ','A','R','N','.',' ','F','o','u','n','d',':',' ','`',
'{','p','a','r','s','e','d','A','r','n','#','r','e','g','i','o','n','}','`','"',',','"','t','y','p',
'e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',':','"','t','r','e','e',
'"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',',','"','e','r','r','o',
'r','"',':','"','P','r','o','v','i','d','e','d',' ','A','R','N',' ','i','s',' ','n','o','t',' ','a',
' ','v','a','l','i','d',' ','C','l','o','u','d','F','r','o','n','t',' ','S','e','r','v','i','c','e',
' ','A','R','N','.',' ','F','o','u','n','d',':',' ','`','{','p','a','r','s','e','d','A','r','n','#',
's','e','r','v','i','c','e','}','`','"',',','"','t','y','p','e','"',':','"','e','r','r','o','r','"',
'}',']',',','"','t','y','p','e','"',':','"','t','r','e','e','"','}',',','{','"','c','o','n','d','i',
't','i','o','n','s','"',':','[',']',',','"','e','r','r','o','r','"',':','"','K','V','S',' ','A','R',
'N',' ','m','u','s','t',' ','b','e',' ','a',' ','v','a','l','i','d',' ','A','R','N','"',',','"','t',
'y','p','e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',':','"','t','r',
'e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',',','"','e','r',
'r','o','r','"',':','"','K','V','S',' ','A','R','N',' ','m','u','s','t',' ','b','e',' ','p','r','o',
'v','i','d','e','d',' ','t','o',' ','u','s','e',' ','t','h','i','s',' ','s','e','r','v','i','c','e',
'"',',','"','t','y','p','e','"',':','"','e','r','r','o','r','"','}',']',',','"','t','y','p','e','"',
':','"','t','r','e','e','"','}',',','{','"','c','o','n','d','i','t','i','o','n','s','"',':','[',']',
',','"','e','r','r','o','r','"',':','"','I','n','v','a','l','i','d',' ','C','o','n','f','i','g','u',
'r','a','t','i','o','n',':',' ','F','I','P','S',' ','i','s',' ','n','o','t',' ','s','u','p','p','o',
'r','t','e','d',' ','w','i','t','h',' ','C','l','o','u','d','F','r','o','n','t','-','K','e','y','V',
'a','l','u','e','S','t','o','r','e','.','"',',','"','t','y','p','e','"',':','"','e','r','r','o','r',
'"','}',']','}','\0'
}};

const char* CloudFrontKeyValueStoreEndpointRules::GetRulesBlob()
{
    return RulesBlob.data();
}

} // namespace CloudFrontKeyValueStore
} // namespace Aws