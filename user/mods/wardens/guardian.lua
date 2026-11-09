-- Shared policy with an authored Vanguard exception, not a C++ identity branch.
return function(self)
    if self.ignore_injured then
        native.flank()
        return
    end
    local patient = nil
    local urgency = 0
    for _, actor in ipairs(native.observe(24)) do
        if actor.ally and actor.healer and actor.health < actor.max_health * 0.8 then
            local score = 1 - actor.health / actor.max_health - actor.distance / 100
            if score > urgency then
                urgency = score
                patient = actor.ref
            end
        end
    end
    if patient then native.guard(patient) else native.advance() end
end
