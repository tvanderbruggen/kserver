# kserver-completion.bash
#
# Autocompletion for the KServer CLI

kserver_call=$1 #./kserver

# Provides a string with the available devices
#
# Use: devices=$(_get_devices)
_get_devices()
{
    local devices=""
    local linecount=0
    
    while read col_id col_name col_ops; 
    do
        if [ ${linecount} -gt 0 ] ; then
            devices+=" $col_name"
        fi
        
        ((linecount++))
    done < <(${kserver_call} status -d)
    
    echo $devices
}

_kserver()
{
    COMPREPLY=()
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD-1]}"

    local opts="host status kill init_tasks -h --help"

    case "${prev}" in
    # HOST
    host)
        local host_opts="-h --help -t --tcp -u --unix -s --status -d --default"
        COMPREPLY=( $(compgen -W "${host_opts}" -- ${cur}) )
        return 0
        ;;
    --tcp|-t) # 'localhost' is a known alias for IP address
        COMPREPLY=( $(compgen -W "localhost" -- ${cur}) )
        return 0
        ;;
    # KILL
    kill)
        local host_opts="-h --help"
        COMPREPLY=( $(compgen -W "${host_opts}" -- ${cur}) )
        return 0
        ;;
    # INIT_TASKS
    init_tasks)
        local init_tasks_opts="-h --help -i --ip_on_leds"
        COMPREPLY=( $(compgen -W "${init_tasks_opts}" -- ${cur}) )
        return 0
        ;;
    # STATUS
    status)
        local status_opts="-h --help -d --devices -s --sessions -p --perfs"
        COMPREPLY=( $(compgen -W "${status_opts}" -- ${cur}) )
        return 0
        ;;
    --devices|-d) 
        local devices=$(_get_devices)
        COMPREPLY=( $(compgen -W "${devices}" -- ${cur}) )
        return 0
        ;;
    *)
        ;;
    esac

    COMPREPLY=($(compgen -W "${opts}" -- ${cur}))  
    return 0
}

complete -F _kserver ${kserver_call}
