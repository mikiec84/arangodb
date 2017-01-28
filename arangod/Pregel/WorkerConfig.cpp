////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/WorkerConfig.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Utils.h"

using namespace arangodb;
using namespace arangodb::pregel;

WorkerConfig::WorkerConfig(DatabaseID dbname, VPackSlice params)
    : _database(dbname) {
  VPackSlice coordID = params.get(Utils::coordinatorIdKey);
  VPackSlice vertexShardMap = params.get(Utils::vertexShardsKey);
  VPackSlice edgeShardMap = params.get(Utils::edgeShardsKey);
  VPackSlice execNum = params.get(Utils::executionNumberKey);
  VPackSlice collectionPlanIdMap = params.get(Utils::collectionPlanIdMapKey);
  VPackSlice globalShards = params.get(Utils::globalShardListKey);
  VPackSlice async = params.get(Utils::asyncMode);
  // VPackSlice userParams = params.get(Utils::userParametersKey);
  if (!coordID.isString() || !edgeShardMap.isObject() ||
      !vertexShardMap.isObject() || !execNum.isInteger() ||
      !collectionPlanIdMap.isObject() || !globalShards.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Supplied bad parameters to worker");
  }
  _executionNumber = execNum.getUInt();
  _coordinatorId = coordID.copyString();
  _asynchronousMode = async.getBool();
  _lazyLoading = params.get(Utils::lazyLoading).getBool();
  //_vertexCollectionName = vertexCollName.copyString();
  //_vertexCollectionPlanId = vertexCollPlanId.copyString();

  for (auto const& pair : VPackObjectIterator(vertexShardMap)) {
    std::vector<ShardID> shards;
    for (VPackSlice shardSlice : VPackArrayIterator(pair.value)) {
      ShardID shard = shardSlice.copyString();
      shards.push_back(shard);
      _localVertexShardIDs.push_back(shard);
    }
    _vertexCollectionShards.emplace(pair.key.copyString(), shards);
  }
      
  prgl_shard_t i = 0;
  for (VPackSlice shard : VPackArrayIterator(globalShards)) {
    ShardID s = shard.copyString();
    _globalShardIDs.push_back(s);
    _shardIDs.emplace(s, i++);
  }
  
  for (auto const& it : VPackObjectIterator(collectionPlanIdMap)) {
    _collectionPlanIdMap.emplace(it.key.copyString(), it.value.copyString());
  }

  for (auto const& pair : VPackObjectIterator(edgeShardMap)) {
    std::vector<ShardID> shards;
    for (VPackSlice shardSlice : VPackArrayIterator(pair.value)) {
      ShardID shard = shardSlice.copyString();
      shards.push_back(shard);
      _localEdgeShardIDs.push_back(shard);
      
      auto const& it = std::find(_localVertexShardIDs.begin(),
                                 _localVertexShardIDs.end(),
                                 shard);
      prgl_shard_t ID = it - _localVertexShardIDs.end();
      _
    }
    _edgeCollectionShards.emplace(pair.key.copyString(), shards);
  }
}

PregelID WorkerConfig::documentIdToPregel(std::string const& documentID) const {
  size_t pos = documentID.find("/");
  if (pos == std::string::npos) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "not a valid document id");
  }
  CollectionID coll = documentID.substr(0, pos);
  std::string _key = documentID.substr(pos + 1);

  auto collInfo =
      Utils::resolveCollection(_database, coll, _collectionPlanIdMap);
  ShardID responsibleShard;
  Utils::resolveShard(collInfo.get(), StaticStrings::KeyString, _key,
                      responsibleShard);

  prgl_shard_t source = this->shardId(responsibleShard);
  return PregelID(source, _key);
}
