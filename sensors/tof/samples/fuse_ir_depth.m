function fused = fuse_ir_depth(depth, ir, ir_max_threshold, ir_min_threshold)

dims = size(depth);
rows = dims(1);
cols = dims(2);

if ~exist('ir_min_threshold', 'var')
    ir_min_threshold = 120;
end
if ~exist('ir_max_threshold', 'var')
    ir_max_threshold = 4000;
end

% fused = depth .* ((ir > ir_min_threshold) & (ir < ir_max_threshold) & ...
fused = depth .* ((ir > ir_min_threshold) & ...
    (depth < max(max(depth))) & reshape((depth > min(min(depth(depth > 0)))), rows, cols));


% Trim points in point cloud that don't have any neighbors (likely noise)

    
end
