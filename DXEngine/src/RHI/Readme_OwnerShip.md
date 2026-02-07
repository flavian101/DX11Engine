# RHI Resource Ownership rules

## Golden Rules

1. **Device owns all GPU resources**: via `std::shared_ptr` The RHI device is the sole owner of all GPU resources (buffers, textures, pipelines, etc.). It is responsible for their creation, management, and destruction.
2. **Commands take raw Pointers** (resources must outlive command execution)
3. **Applications hold `std::shared_ptr`** to keep resources alive as long as needed. The application should maintain `std::shared_ptr` references to any resources it uses, ensuring they remain valid for the duration of their use.
4. **Never store raw pointers long-term** in application code. Always use `std::shared_ptr` to manage resource lifetimes and avoid dangling pointers.
