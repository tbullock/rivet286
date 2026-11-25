msg_report: db 'PMTEST 0100', 13, 10, 0
newline: db 13, 10, 0
report_names:
    dw name_mode, name_seed, name_result, name_checks, name_vector, name_error
    dw name_ip, name_cs, name_resume, name_msw, name_real, name_calls
    dw name_memory
name_mode: db 'MODE=', 0
name_seed: db 'SEED=', 0
name_result: db 'RESULT=', 0
name_checks: db 'CHECKS=', 0
name_vector: db 'VECTOR=', 0
name_error: db 'ERROR=', 0
name_ip: db 'IP=', 0
name_cs: db 'CS=', 0
name_resume: db 'RESUME=', 0
name_msw: db 'MSW=', 0
name_real: db 'REAL=', 0
name_calls: db 'CALLS=', 0
name_memory: db 'MEMORY=', 0
name_status: db 'STATUS=', 0
name_value: db 'VALUE=', 0
name_tests: db 'TESTS=', 0
name_return: db 'RETURN=', 0
