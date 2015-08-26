function iBreak = BREAK_THRESHOLD(Y,THD)
    % ***COMPLETED***
    %
    % FOR AC SIGNALS WITH ZERO DC OFFSET!
    %
    % This function evaluates an array Y from left-to-right.
    % The 1st Y that breaks a positive or negative threshold
    % has its index returned.
    %
    % THD = Threshold
    
    iBreak = -1;
    
    i = 1;
    while ( i < length(Y) - 1)
        yTest = abs(Y(i));

        if ( yTest > THD )
            iBreak = i;
            i = length(Y);
        end

        i = i+1;
    end

    if ( iBreak == -1 )
        disp('Threshold wasnt broken')
    end

end

