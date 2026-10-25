from collections import deque
from BaseClasses import CollectionState, MultiWorld
from worlds.AutoWorld import LogicMixin
from .LogicHelpers import IsChild, IsAdult, child_age_dependent_rules, adult_age_dependent_rules
from .Enums import Regions, Ages
from .oot3d_data import GAME
import copy


class SohAgeLogic(LogicMixin):
    def init_mixin(self, parent: MultiWorld):
        game = GAME
        self._oot3d_stale = {player: True for player in parent.worlds.keys()
                           if parent.worlds[player].game == game}
        players = parent.get_game_groups(game) + parent.get_game_players(game)
        self._oot3d_child_reachable_regions = {
            player: set() for player in players}
        self._oot3d_adult_reachable_regions = {
            player: set() for player in players}
        self._oot3d_child_blocked_regions = {player: set() for player in players}
        self._oot3d_adult_blocked_regions = {player: set() for player in players}
        self._oot3d_age = {player: Ages.null for player in players}

    def copy_mixin(self, ret) -> CollectionState:
        ret._oot3d_stale = {player: stale for player,
                          stale in self._oot3d_stale.items()}
        ret._oot3d_child_reachable_regions = {player: copy.copy(
            regions) for player, regions in self._oot3d_child_reachable_regions.items()}
        ret._oot3d_adult_reachable_regions = {player: copy.copy(
            regions) for player, regions in self._oot3d_adult_reachable_regions.items()}
        ret._oot3d_child_blocked_regions = {player: copy.copy(
            regions) for player, regions in self._oot3d_child_blocked_regions.items()}
        ret._oot3d_adult_blocked_regions = {player: copy.copy(
            regions) for player, regions in self._oot3d_adult_blocked_regions.items()}
        ret._oot3d_age = {player: age for player, age in self._oot3d_age.items()}
        return ret

    def _oot3d_invalidate(self, player):
        self._oot3d_child_reachable_regions[player] = set()
        self._oot3d_adult_reachable_regions[player] = set()
        self._oot3d_child_blocked_regions[player] = set()
        self._oot3d_adult_blocked_regions[player] = set()
        self._oot3d_stale[player] = True

    def _oot3d_update_age_reachable_regions(self, player):
        self._oot3d_stale[player] = False
        for age in [Ages.CHILD, Ages.ADULT]:
            self._oot3d_age[player] = age
            start = self.multiworld.get_region(
                Regions.ROOT, player)  # type: ignore

            if age == Ages.CHILD:
                reachable = self._oot3d_child_reachable_regions[player]
                blocked = self._oot3d_child_blocked_regions[player]
                queue = deque(self._oot3d_child_blocked_regions[player])
            else:
                reachable = self._oot3d_adult_reachable_regions[player]
                blocked = self._oot3d_adult_blocked_regions[player]
                queue = deque(self._oot3d_adult_blocked_regions[player])

            # init on first call
            if start not in reachable:
                reachable.add(start)
                blocked.update(start.exits)
                queue.extend(start.exits)

            # run breadth first search
            while queue:
                connection = queue.popleft()
                new_region = connection.connected_region
                if new_region is None:
                    continue
                if new_region in reachable:
                    blocked.remove(connection)
                elif connection.can_reach(self):
                    reachable.add(new_region)
                    blocked.remove(connection)
                    blocked.update(new_region.exits)
                    queue.extend(new_region.exits)
                    self.path[new_region] = (new_region.name, self.path.get(
                        connection, None))  # type: ignore

    def _oot3d_can_reach_as_age(self, region: Regions, age: Ages, player: int):
        if self._oot3d_age[player] == Ages.null:
            # first layer of recursion
            self._oot3d_age[player] = age
            can_reach = self.multiworld.get_region(
                region.value, player).can_reach(self)  # type: ignore
            self._oot3d_age[player] = Ages.null
            return can_reach
        return self._oot3d_age[player] == age
